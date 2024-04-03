//
// Created by OMEN on 2024/2/9.
//

#ifndef FASTERTRANSFORMER_CONFIG_H
#define FASTERTRANSFORMER_CONFIG_H

#pragma once

#include <chrono>
#include <cstdlib>

#define TimePoint std::chrono::time_point<std::chrono::high_resolution_clock>
#define NOW std::chrono::high_resolution_clock::now()
namespace Config{
    const double STARVATION_TIME = 5;
    const int MLFQ_QUEUE_NUMBE = 8;
    const int Quantum_Ratio = 2;
    const int MAX_BATCH_SIZE = 16;
    const bool FAST_SERVE_SCHEDULER = true;
//    const int K = 20;

    static double interval(TimePoint start, TimePoint end = NOW){
        return (double)(end-start).count()/(1000.0 * 1000.0);
    }
}
#define DURATION(start) Config::interval(start)


#endif //FASTERTRANSFORMER_CONFIG_H
