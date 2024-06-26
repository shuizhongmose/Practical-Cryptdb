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
            pthread_cond_wait(&queue_cond, &queue_mutex);
        }
        if (stop && task_queue.empty()) {
            pthread_mutex_unlock(&queue_mutex);
            return;
        }
        ThdTask task = std::move(task_queue.front());
        task_queue.pop_front();
        pthread_mutex_unlock(&queue_mutex);
        // std::cout << "begin to run task func ......";
        if(task.func) {
            std::vector<Item *> result = task.func();
            // std::cout << "Ohoo, run task done";
            pthread_mutex_lock(&queue_mutex);
            results.push_back(std::make_pair(task.index, result));
            pthread_mutex_unlock(&queue_mutex);
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
