#include <main/thread_pool.hh>
#include <util/cryptdb_log.hh>

ThreadPool::ThreadPool(unsigned long num_threads) : stop(false) {
    pthread_mutex_init(&queue_mutex, NULL);
    pthread_mutex_init(&stop_mutex, nullptr);
    pthread_cond_init(&queue_cond, NULL);
    for (size_t i = 0; i < num_threads; ++i) {
        pthread_t thread;
        pthread_create(&thread, NULL, workerWrapper, this);
        workers.push_back(thread);
    }
}

ThreadPool::~ThreadPool() {
    waitForCompletion();
    // clear all queue and list
    for (auto& pair : results) {
        pair.second.clear();
    }
    results.clear();
    task_queue.clear();
    workers.clear();
    
    pthread_cond_destroy(&queue_cond);
    pthread_mutex_destroy(&queue_mutex);
    pthread_mutex_destroy(&stop_mutex);
}

void ThreadPool::addTask(ThdTask task) {
    pthread_mutex_lock(&queue_mutex);
    task_queue.push_back(std::move(task));
    pthread_cond_signal(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);
}

void ThreadPool::worker() {
    while (true) {
        pthread_mutex_lock(&queue_mutex);
        while (task_queue.empty() && !stop) {
            // LOG(debug) << "Waiting for call up";
            pthread_cond_wait(&queue_cond, &queue_mutex);
        }
        if (stop && task_queue.empty()) {
            pthread_mutex_unlock(&queue_mutex);
            // LOG(debug) << "finish task and return";
            return;
        }
        ThdTask task = std::move(task_queue.front());
        task_queue.pop_front();
        pthread_mutex_unlock(&queue_mutex);
        if(task.func) {
            try {
                // LOG(debug) << "begin to run task func ......";
                std::vector<Item *> result = task.func();
                pthread_mutex_lock(&queue_mutex);
                results.push_back(std::make_pair(task.index, result));
                // LOG(debug) << "Oh yeah, run task done and push result";
                pthread_mutex_unlock(&queue_mutex);
            } catch  (const std::exception &e) {
                 LOG(error) << "Exception in task.func: " << e.what();
                continue; // Skip to next iteration
            }
        } else {
            LOG(error) << "Thread " << task.index << " is null";
        }
    }
}

void* ThreadPool::workerWrapper(void* context) {
    ((ThreadPool*)context)->worker();
    return NULL;
}

void ThreadPool::waitForCompletion() {
    setStop();
    pthread_mutex_lock(&queue_mutex);
    pthread_cond_broadcast(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);

    for (pthread_t thread : workers) {
        pthread_join(thread, NULL);
    }
}

void ThreadPool::getSortedResults() {
    std::sort(results.begin(), results.end(), Comparator());
}

bool ThreadPool::setStop() {
    pthread_mutex_lock(&stop_mutex);
    stop = true;
    pthread_mutex_unlock(&stop_mutex);
    return stop;
}

void ThreadPool::clearResults() {
    pthread_mutex_lock(&queue_mutex);
    for (auto& pair : results) {
        pair.second.clear();
    }
    results.clear();
    pthread_mutex_unlock(&queue_mutex);
}

void ThreadPool::restart(){
    pthread_mutex_lock(&stop_mutex);
    stop = false;
    pthread_mutex_unlock(&stop_mutex);

    for (size_t i = 0; i < workers.size(); ++i) {
        pthread_t thread;
        pthread_create(&thread, NULL, workerWrapper, this);
        workers[i] = thread;
    }
}