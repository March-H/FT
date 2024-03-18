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
    mtx.lock();
    if (new_jobs.empty()) {
        mtx.unlock();
        return true;
    }
    mtx.unlock();
    return false;
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
//    mtx.lock();
    if (ready_jobs.empty()) {
//        mtx.unlock();
        return true;
    }
//    mtx.unlock();
    return false;
}

bool Scheduler::check_mlfq_empty(int id) {
//    mtx.lock();
    if (mlfq[id].empty()) {
        mtx.unlock();
        return true;
    }
//    mtx.unlock();
    return false;
}

size_t Scheduler::get_ready_jobs_size() {
//    mtx.lock();
    size_t tmp = ready_jobs.size();
//    mtx.unlock();
    return tmp;
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
//        std::this_thread::sleep_for(std::chrono::milliseconds(1));

//        FT_LOG_INFO("Process newly arrived jobs.");
        while (!check_new_jobs_empty()) {
            mtx.lock();
            job = new_jobs.front();
            new_jobs.pop_front();
            mtx.unlock();
            int init_priority;
            int init_quantum;
            get_initial_priority_quantum(job->input_length,init_priority,init_quantum);
//            FT_LOG_INFO("Scheduler got job(%d), length(%d), priority(%d).", new_job->job_id, new_job->input_length,
//                   init_priority);
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

//        FT_LOG_INFO("Schedule jobs to be executed.");
        for (int i = 0; i < Config::MLFQ_QUEUE_NUMBE; ++i) {
            while (!check_mlfq_empty(i) && get_ready_jobs_size() + running_jobs.size() < Config::MAX_BATCH_SIZE) {
                job = mlfq[i].front();
                mlfq[i].pop_front();
                ready_jobs.push_back(job);
//                FT_LOG_INFO("Send job(%d) to be executed.", job->job_id);
            }
        }

//        FT_LOG_INFO("Wait and demote preempted jobs.");
        auto it = running_jobs.begin();
        while(it != running_jobs.end()){
            if(!(*it)->finish_iteration()){
                ++it;
                continue;
            }else{
                job = (*it);
                running_jobs.erase(it);
                if (!job->check_job_done()){
                    int demoted_priority = get_next_priority(job->job_priority);
                    job->update_job_priority(demoted_priority, quantum_of_mlfq[demoted_priority]);
                    mtx.lock();
                    mlfq[demoted_priority].push_back(job);
                    mtx.unlock();
                }else{
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

//        FT_LOG_INFO("Start scheduled jobs.\n");
        while (!check_ready_jobs_empty()) {
            job = ready_jobs.front();
            ready_jobs.pop_front();
            job->start_iteration();
//            FT_LOG_INFO("Job(%d) start iteration.", job->job_id);
            running_jobs.push_back(job);
        }
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
//        mtx.lock();
//        new_jobs.push_back(job);
//        mtx.unlock();
//    }

//    FT_LOG_INFO("Scheduler add job-{%d}.",jobId);
//    FT_LOG_INFO("third step.");
    Job* job = new Job(jobId,inputLength);
//    FT_LOG_INFO("fourth step.");
    mtx.lock();
    new_jobs.push_back(job);
    mtx.unlock();

    return job;
}

Scheduler::Scheduler(int mode): scheduler_mode(mode){
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
