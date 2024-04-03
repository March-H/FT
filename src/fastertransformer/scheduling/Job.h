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
#include "Channel.h"

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
    bool KV_loaded = false;
    bool loading = false;
    Channel<int> forward_chan, cache_chan;

    std::mutex mtx; // job - Parallel

    Job(int job_id, int input_length);
    ~Job();

    [[nodiscard]] bool is_starving();
    void update_job_priority(int priority, int quantum);

    void generated_one_token(bool done = false);

    void start_iteration();
    void upload();
    bool check_KV_loaded();
    void set_KV_loaded(bool flag);
    [[nodiscard]] bool finish_iteration();
    [[nodiscard]] bool check_job_done();

};

//std::vector<double> generatePoissonDistribution(int n, double lambda);


#endif //FASTERTRANSFORMER_JOB_H
