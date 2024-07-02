#pragma once

#include <pthread.h>
#include <iostream>
#include <vector>
#include <list>
#include <functional>
#include <memory>
#include <algorithm>

struct Item;

struct ThdTask {
    std::function<std::vector<Item *>()> func;
    size_t index;
};

class ThreadPool {
public:
    struct Comparator {
        bool operator()(const std::pair<size_t, std::vector<Item*>>& a,
                        const std::pair<size_t, std::vector<Item*>>& b) const {
            return a.first < b.first;
        }
    };

    std::vector<std::pair<size_t, std::vector<Item *>>> results;

    ThreadPool(unsigned long num_threads);
    ~ThreadPool();
    void addTask(ThdTask task);
    void worker();
    static void* workerWrapper(void* context);
    void waitForCompletion();
    void getSortedResults();
    bool setStop();
    void clearResults();
    void restart();


private:
    pthread_mutex_t stop_mutex;
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_cond;
    std::list<ThdTask> task_queue;
    bool stop;
    std::vector<pthread_t> workers;
};
