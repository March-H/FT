//
// Created by OMEN on 2024/2/9.
//

#include "Job.h"
#include <vector>
#include <random>
#include "config.h"
#include <thread>

Job::Job(const int job_id, int input_length) : job_id(job_id), input_length(input_length){
//    FT_LOG_INFO("third-1 step.");
    last_token_done_time = NOW;
//    FT_LOG_INFO("third-2 step.");
    recieveTime = NOW;
//    FT_LOG_INFO("third-3 step.");
//    FT_LOG_INFO("Job-%d initialize.", job_id);
}

Job::~Job() {
//    FT_LOG_INFO("Job-%d done.", job_id);
}

bool Job::is_starving() {
    std::unique_lock<std::mutex> lock(mtx);
    double duration = DURATION(this->last_token_done_time);
    if (duration/1000.0 >= Config::STARVATION_TIME) {// Config::STARVATION_TIME
//        FT_LOG_INFO("Job-%d is starving, promote it.", job_id);
        return true;
    }
    return false;
}

void Job::update_job_priority(int priority, int quantum) {
//    this->mtx.lock();
//    FT_LOG_INFO("Job-%d UPDATE: priority & quantum = {%d} {%d}", job_id, priority, quantum);
    std::unique_lock<std::mutex> lock(mtx);
    this->job_priority = priority;
    this->job_quantum = quantum;
//    this->mtx.unlock();
}

void Job::generated_one_token(bool done) {
    // return 1 to call FT unlock
    // 作业quantum未用完，不抢占锁
    // 作业done了，不抢占锁，但是需要对方释放锁
    // 作业quantum为0，抢占锁，但是要FT等待，等调度器判断是否要卸载KV Cache（如果不在ready队列中，则卸载，否则调度器释放锁）
    FT_LOG_INFO("[generated_one_token] enter.");
    std::unique_lock<std::mutex> lock(mtx);
    last_token_done_time = NOW;

    generated_tokens++;
    job_quantum-=1;
    if(done){
        is_job_done = true;
        job_quantum = 0;
    }else if(job_quantum){
        FT_LOG_INFO("[generated_one_token] keep iterating.");
        cache_chan.send(1);
        forward_chan.send(1);
    }
    FT_LOG_INFO("[generated_one_token] left.");
//    else if(job_quantum == 0){
//        lock_forward.lock();
//        lock_cache.lock();
//        return 1;
//    }
//    FT_LOG_INFO("Job-%d finish generated the %d token.",job_id,generated_tokens-1);
//    return 0;
}

bool Job::check_KV_loaded(){
    std::unique_lock<std::mutex> lock(mtx);
    return KV_loaded;
};

void Job::upload() {
    std::unique_lock<std::mutex> lock(mtx);
    if(KV_loaded) return;
    cache_chan.send(1);

//    FT_LOG_INFO("3333333333333333");
//    lock_cache.unlock();
//    cv_cache_join.notify_one();
}

void Job::start_iteration() {
    if(this->job_quantum<=0){
//        FT_LOG_ERROR("Quantum for the task-%d is not enough.",job_id);
    }
    // TODO: whether to check if have the lock_forward?
    else{
//        FT_LOG_INFO("Job-%d free lock and start iteration.",job_id);
        while(!check_KV_loaded()){
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        forward_chan.send(1);
//        lock_forward.unlock();
//        cv_gpt_forward.notify_one();
    }
//    mtx.lock();
//    this->is_iteration = true;
//    int record_generated_tokens = this->generated_tokens;
//    std::thread generateTokens([this, record_generated_tokens]() {
//        generate_one_token(record_generated_tokens);
//    });
//    generateTokens.detach();
//    this->mtx.unlock();
//    if(mode == 0){// FastServe妯″紡
//        // 寮€濮嬩竴涓鏃跺櫒
//        std::thread timerThread([this,record_generated_tokens]() {
//            //            printf("Job(%d) start a %dms timer.\n", job_id, job_quantum * 20 * 2);
//            std::this_thread::sleep_for(std::chrono::milliseconds(this->job_quantum * 20 * 2)); // 姣忎釜quantum鏄?0ms
//            this->mtx.lock();
//            if (record_generated_tokens == this->generated_tokens && this->is_iteration) {
//                this->is_iteration = false;
//                //                cv.notify_one();
//                //                printf("Job(%d) timeout.\n", job_id);
//            }
//            this->mtx.unlock();
//        });
//        timerThread.detach();
//    }
}

bool Job::finish_iteration(){
    std::unique_lock<std::mutex> lock(mtx);
    return job_quantum==0;
}

bool Job::check_job_done(){
    std::unique_lock<std::mutex> lock(mtx);
    return is_job_done;
}

void Job::set_KV_loaded(bool flag) {
    std::unique_lock<std::mutex> lock(mtx);
    KV_loaded = flag;
}

//void Job::wait_iteration() {
//    std::unique_lock<std::mutex> lock(mtx);
//    // 绛夎秴鏃舵垨鑰呬竴娆¤凯浠ｇ粨鏉?
//    printf("Job(%d) start wait iteration.\n", job_id);
//    cv.wait(lock, [this]() {
//        return !is_iteration;
//    });
//    printf("Job(%d) finish wait iteration.\n", job_id);
//}

std::vector<double> generatePoissonDistribution(int n, double lambda) {
    std::vector<double> result;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::poisson_distribution<int> poisson(lambda);

    for (int i = 0; i < n; ++i) {
        int value = poisson(gen);
        result.push_back(static_cast<double>(value));
    }

    return result;
}