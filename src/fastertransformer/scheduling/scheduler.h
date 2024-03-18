//
// Created by OMEN on 2024/2/9.
//

#ifndef FASTERTRANSFORMER_SCHEDULER_H
#define FASTERTRANSFORMER_SCHEDULER_H

#include "Job.h"
#include "config.h"
#include <queue>
#include <vector>
#include "List.h"
#include "src/fastertransformer/utils/logger.h"
#include <thread>

struct Scheduler{
public:
    Scheduler(int mode);
    ~Scheduler();

    std::thread _scheduler;
    const int scheduler_mode;

    std::deque<Job*> new_jobs;
    List<Job*> running_jobs;
    std::deque<Job*> ready_jobs;
    std::deque<Job*> mlfq[Config::MLFQ_QUEUE_NUMBE];

    int quantum_of_mlfq[Config::MLFQ_QUEUE_NUMBE];
    int total_job = 0;
    double JCT = 0;
    bool is_scheduling=false;

    std::mutex mtx;

    void Start();
    void FastServe_scheduler();
    static void remove_job(Job* job);
    Job* allocate_new_job(int jobId, int inputLength);

    void get_initial_priority_quantum(int length,int &priority, int &quantum);
    int get_next_priority(int priority);
    int get_next_iter_time(int generated_tokens,int priority);

    bool check_new_jobs_empty();
//    bool check_running_jobs_empty();
    bool check_ready_jobs_empty();
    bool check_mlfq_empty(int id);
    size_t get_ready_jobs_size();

//    Job *jobMaker(const int &input_length, const int &need_tokens);

};

#endif //FASTERTRANSFORMER_SCHEDULER_H
