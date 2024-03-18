//
// Created by OMEN on 2024/2/9.
//

#ifndef FASTERTRANSFORMER_JOB_H
#define FASTERTRANSFORMER_JOB_H

#include <mutex>
#include <condition_variable>
#include <vector>
#include "config.h"
#include "src/fastertransformer/utils/logger.h"

struct Job{
public:
    const int job_id;
    int input_length;

    TimePoint last_token_done_time;
    TimePoint recieveTime;

    int generated_tokens = 0;
    bool is_job_done = false;
    int job_quantum = 0;
    int job_priority = 0;


    std::mutex mtx, mtx_forward; // job - Parallel
    std::unique_lock<std::mutex> lock_forward; // job - Scheduler
    std::condition_variable cv_gpt_forward, cv_job_join;

    Job(int job_id, int input_length);
    ~Job();

    [[nodiscard]] bool is_starving() const;
    void update_job_priority(int priority, int quantum);

    void generated_one_token(bool done = false);

    void start_iteration();
    [[nodiscard]] bool finish_iteration();
    [[nodiscard]] bool check_job_done();

};

//std::vector<double> generatePoissonDistribution(int n, double lambda);


#endif //FASTERTRANSFORMER_JOB_H
