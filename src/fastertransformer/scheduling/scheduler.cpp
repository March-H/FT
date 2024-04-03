//
// Created by OMEN on 2024/2/9.
//
#include "scheduler.h"
#include <thread>
#include <iostream>
#include "Job.h"
#include <fstream>
#include <filesystem>
#include <cmath>
#include "config.h"

bool Scheduler::check_new_jobs_empty() {
    std::unique_lock<std::mutex> lock(mtx);
    return new_jobs.empty();
}

//bool Scheduler::check_running_jobs_empty() {
//    mtx.lock();
//    if (running_jobs.empty()) {
//        mtx.unlock();
//        return true;
//    }
//    mtx.unlock();
//    return false;
//}

bool Scheduler::check_ready_jobs_empty() {
    std::unique_lock<std::mutex> lock(mtx);
    return ready_jobs.empty();
}

bool Scheduler::check_mlfq_empty(int id) {
    std::unique_lock<std::mutex> lock(mtx);
    return mlfq[id].empty();
}

size_t Scheduler::get_ready_jobs_size() {
    std::unique_lock<std::mutex> lock(mtx);
    return ready_jobs.size();
}

void Scheduler::get_initial_priority_quantum(int length,int &priority, int &quantum) {
    int num_of_quantum = ceil((0.00003*length*length+0.06*length+4.66)/20)+1;
    int tmp = num_of_quantum;
    priority=0;
    for(;priority < Config::MLFQ_QUEUE_NUMBE;++priority){
        if(num_of_quantum<=quantum_of_mlfq[priority]) break;
//        printf("%d %d \n",priority, num_of_quantum);
    }
//    printf("%d %d %d \n", priority, quantum_of_mlfq[priority], tmp);
    quantum = quantum_of_mlfq[priority] - tmp + 1;
}

int Scheduler::get_next_priority(int priority) {
    return std::min(priority + 1, Config::MLFQ_QUEUE_NUMBE-1);
}

int Scheduler::get_next_iter_time(int generated_tokens, int priority) {
    if (generated_tokens == 0) {
        return 1;
    }
    return quantum_of_mlfq[priority];
}

void Scheduler::remove_job(Job *job) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2560*2));
    delete job;
    job = nullptr;
}

void Scheduler::FastServe_scheduler(){
    std::queue<Job*> tmp;
    Job *job;
    while (is_scheduling) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

//        FT_LOG_INFO("Process newly arrived jobs.");
        while (!check_new_jobs_empty()) {
            std::unique_lock<std::mutex> lock(mtx);
            job = new_jobs.front();
            new_jobs.pop_front();
            int init_priority;
            int init_quantum;
            get_initial_priority_quantum(job->input_length,init_priority,init_quantum);
            FT_LOG_INFO("Scheduler got job(%d), length(%d), priority(%d).", job->job_id, job->input_length,
                   init_priority);
            job->update_job_priority(init_priority, init_quantum);
            this->mlfq[init_priority].push_back(job);
        }

//        FT_LOG_INFO("Promote starved jobs.");
        for (int i = 1; i < Config::MLFQ_QUEUE_NUMBE; ++i) {
            while(!mlfq[i].empty()){
                job = mlfq[i].front();
                mlfq[i].pop_front();
                if (job->is_starving()) {
//                    FT_LOG_INFO("Job(%d) is starving, promote to hightest priority.", job->job_id);
                    job->update_job_priority(0, get_next_iter_time(job->generated_tokens, job->job_priority));
                    mlfq[0].push_back(job);
                }else{
                    tmp.push(job);
                }
            }
            while(tmp.size()){
                mlfq[i].push_back(tmp.front());
                tmp.pop();
            }
        }

        //    FT_LOG_INFO("Wait and demote preempted jobs.");
        auto it = running_jobs.begin();
        while(it != running_jobs.end()){
//            FT_LOG_INFO("Job(%d) check finish iteration?",job->job_id);
            if(!(*it)->finish_iteration()){
//                FT_LOG_INFO("Job(%d) don't finish iteration.",job->job_id);
                ++it;
                continue;
            }else{
                FT_LOG_INFO("Job(%d) finish iteration.",job->job_id);
                job = (*it);
                running_jobs.erase(it);
                FT_LOG_INFO("Job(%d) check job done?",job->job_id);
                if (!job->check_job_done()){
                    int demoted_priority = get_next_priority(job->job_priority);
                    job->update_job_priority(demoted_priority, quantum_of_mlfq[demoted_priority]);
                    int num=0;
                    for(int i=0;i<=demoted_priority;++i){
                        num += mlfq[i].size();
                    }
                    if(num + 1 <= MAX_READY_SIZE - get_ready_jobs_size()){
                        FT_LOG_INFO("Job(%d) needn't to offload KV Cache.",job->job_id);
                        job->cache_chan.send(1);
                    }else{
                        FT_LOG_INFO("Job(%d) should offload KV Cache.",job->job_id);
//                        job->Cache_should_offload.send(1);
                        job->cache_chan.send(0);
                    }
                    mlfq[demoted_priority].push_back(job);
                }else{
                    FT_LOG_INFO("Job(%d) finish.",job->job_id);
                    job->cache_chan.send(1);
                    double duration = DURATION(job->recieveTime);
                    JCT = total_job * JCT + duration/1000.0;
                    total_job++;
                    JCT = JCT / total_job;
//                    outputFile<<total_job<<" "<<JCT<<'\n';
                    std::thread del([job, this](){
                        remove_job(job);
                    });
                    del.detach();

                }
            }
        }

//        FT_LOG_INFO("Schedule jobs to be executed.");
        for (int i = 0; i < Config::MLFQ_QUEUE_NUMBE; ++i) {
            while (!check_mlfq_empty(i) && get_ready_jobs_size() < MAX_READY_SIZE) {
                job = mlfq[i].front();
                job->upload();
//                FT_LOG_INFO("upload KV Cache of job %d.",job->job_id);
                mlfq[i].pop_front();
                ready_jobs.push_back(job);
                FT_LOG_INFO("Send job(%d) to be executed.", job->job_id);
            }
        }


//        FT_LOG_INFO("Start scheduled jobs.\n");
        auto ti = ready_jobs.begin();
        bool needExpand = running_jobs.size()<Config::MAX_BATCH_SIZE;
        while(ti != ready_jobs.end() && running_jobs.size()<Config::MAX_BATCH_SIZE){
            if((*ti)->check_KV_loaded()){
                needExpand = false;
                ready_jobs.erase(ti);
                (*ti)->start_iteration();
                FT_LOG_INFO("Job(%d) start iteration.", (*ti)->job_id);
                running_jobs.push_back((*ti));
            }else{
                ti++;
            }
        }
        // if running_jobs have vacancy but ready_jobs are full and no KV_loaded job can be iterated
        if(needExpand && get_ready_jobs_size()==MAX_READY_SIZE){
            MAX_READY_SIZE++;
        }
        else // if KV_loaded job in ready_jobs exceed Config::MAX_BATCH_SIZE
        {
            ti = ready_jobs.begin();
            int cnt=0;
            while(ti != ready_jobs.end()){
                if((*ti)->check_KV_loaded()){
                    cnt++;
                }
            }
            if(cnt>Config::MAX_BATCH_SIZE){
                MAX_READY_SIZE--;
            }
        }
        /*while (!check_ready_jobs_empty() && running_jobs.size()<Config::MAX_BATCH_SIZE) {
            job = ready_jobs.front();
            ready_jobs.pop_front();
            job->start_iteration();
            FT_LOG_INFO("Job(%d) start iteration.", job->job_id);
            running_jobs.push_back(job);
        }*/
    }
}

void Scheduler::Start() {
//    FT_LOG_INFO("Start scheduler.");
//    std::string filePath = R"(E:\DeskTop\yan0\Test\dataset\1.txt)";
//    std::ofstream outputFile(filePath);
//    is_scheduling = true;
    switch(scheduler_mode){
        case 0:
            FastServe_scheduler();
            break;
        default:
            FastServe_scheduler();
            break;
    }

}

/*Job* Scheduler::jobMaker(const int &input_length, const int &need_tokens) {
    static int id = 0;
//    int input_length = rand() % 1000 + 10;
//    int need_tokens = rand() % 50 + 1;
    Job *job = new Job(id, input_length, need_tokens);
    job->recieveTime = Clock::now();
    job->last_token_done_time = Clock::now();
    id++;
    return job;
}*/

Job* Scheduler::allocate_new_job(int jobId, int inputLength) {
//    std::string filePath = R"(E:\DeskTop\yan0\Test\dataset\input_output235.in)";
//    std::ifstream inputFile(filePath);
//    int input_length,output_length;
//    std::vector<double> poissonValues = generatePoissonDistribution(n,10);
//    for (int i = 0; i < n; ++i) {
//        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int64_t>(1000.0/poissonValues[i])));
//        inputFile>>input_length>>output_length;
//        Job *job = jobMaker(input_length,output_length);
//        lock_mtx.lock();
//        new_jobs.push_back(job);
//        lock_mtx.unlock();
//    }

//    FT_LOG_INFO("Scheduler add job-{%d}.",jobId);
//    FT_LOG_INFO("third step.");
    FT_LOG_INFO("create job %d",jobId);
    std::unique_lock<std::mutex> lock(mtx);
    Job* job = new Job(jobId,inputLength);
//    FT_LOG_INFO("fourth step.");
    new_jobs.push_back(job);
    FT_LOG_INFO("finish create job %d",jobId);

    return job;
}

Scheduler::Scheduler(int mode): scheduler_mode(mode), mtx(){
//    FT_LOG_INFO("[Scheduler] scheduler initialization.");
    int quantum = 1;
    for(int i=0;i<Config::MLFQ_QUEUE_NUMBE;++i){
        quantum_of_mlfq[i] = quantum;
        quantum *= Config::Quantum_Ratio;
    }
    is_scheduling = true;

    _scheduler = std::thread(&Scheduler::Start, this);
}

Scheduler::~Scheduler() {
    is_scheduling = false;
    if(_scheduler.joinable()){
        _scheduler.join();
    }
//    FT_LOG_INFO("[Scheduler] scheduler stopped.");
}
