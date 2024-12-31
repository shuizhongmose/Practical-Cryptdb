#pragma once

/**
 * @brief 论文《Popa R A, Redfield C M S, Zeldovich N, et al. CryptDB: Protecting confidentiality with encrypted query processing[C]//
 * Proceedings of the twenty-third ACM symposium on operating systems principles. 2011: 85-100.》中参考文件[25]的实现方法
 * 基于超几何采样器的OPE实现
 * 使用该OPE时，用ope-KS88.*替换ope.*的内容
 * 
 */

#undef max
#undef min

#include <string>
#include <map>

#include <unordered_map>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <utility>
#include <algorithm>
#include <ctime>
#include <iostream>
#include <chrono>

#include <crypto/prng.hh>
#include <crypto/aes.hh>
#include <crypto/sha.hh>
#include <util/cryptdb_log.hh>
#include <util/util.hh>
#include <NTL/ZZ.h>

using namespace std;
using namespace NTL;

class ope_domain_range {
 public:
    ope_domain_range(const NTL::ZZ &d_arg,
                     const NTL::ZZ &r_lo_arg,
                     const NTL::ZZ &r_hi_arg)
        : d(d_arg), r_lo(r_lo_arg), r_hi(r_hi_arg) {}
    NTL::ZZ d, r_lo, r_hi;
};

// Helper function to get cache size from environment variable
inline size_t get_cache_size_from_env(const std::string& env_var_name, size_t default_size) {
    const char* env_val = std::getenv(env_var_name.c_str());
    if (env_val) {
        try {
            return std::stoul(env_val);  // Convert string to size_t
        } catch (const std::invalid_argument& e) {
            std::cerr << "Invalid cache size in environment variable " << env_var_name
                      << ": " << env_val << ". Using default size " << default_size << "." << std::endl;
        } catch (const std::out_of_range& e) {
            std::cerr << "Cache size in environment variable " << env_var_name
                      << " is out of range: " << env_val << ". Using default size " << default_size << "." << std::endl;
        }
    }
    return default_size;
}

// 为 NTL::ZZ 实现 hash 特化
namespace std {
    template <>
    struct hash<NTL::ZZ> {
        size_t operator()(const NTL::ZZ& z) const {
            return std::hash<std::string>()(StringFromZZ(z));  // 使用字符串的哈希函数
        }
    };
}

template <typename Key, typename Value>
class LRUCache {
    size_t max_size;
    std::unordered_map<Key, long> access_count;  // 使用 unordered_map 记录每个 key 的访问次数
    std::unordered_map<Key, Value> cache_map;    // 使用 unordered_map 存储缓存的键值对

    // 线程相关
    std::thread eviction_thread;
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> stop_thread;

    // 淘汰参数
    std::chrono::milliseconds eviction_interval;

public:
    // 构造函数，接收最大容量和淘汰间隔（默认30秒）
    LRUCache(size_t size, std::chrono::milliseconds interval = std::chrono::milliseconds(30000))
        : max_size(size), stop_thread(false), eviction_interval(interval) {
        // 启动后台淘汰线程
        eviction_thread = std::thread(&LRUCache::eviction_worker, this);
    }

    ~LRUCache() {
        // 停止后台线程
        stop_thread = true;
        cv.notify_all();
        if (eviction_thread.joinable()) {
            eviction_thread.join();
        }
    }

    // 获取值，如果存在则返回 true 并通过引用参数返回值
    bool get(const Key& key, Value& value) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = cache_map.find(key);
        if (it == cache_map.end()) {
            return false;
        }
        // 增加访问计数
        access_count[key]++;
        value = it->second;
        return true;
    }

    // 安全插入：检查后插入
    void safe_put(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = cache_map.find(key);
        if (it == cache_map.end()) {
            // 插入新元素
            cache_map[key] = value;
            access_count[key] = 1;  // 新插入元素的访问计数为1
        } else {
            // 更新已有元素的值并增加访问计数
            it->second = value;
            access_count[key]++;
        }
        // 不在这里执行淘汰，后台线程会定时检查
    }

    // 非安全插入，不做检查直接插入，默认 key 不存在
    void put(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lock(mtx);
        // 插入或更新缓存
        cache_map[key] = value;
        access_count[key] = 1;  // 新插入元素的访问计数为1
        // 不在这里执行淘汰，后台线程会定时检查
    }

    // 清空缓存
    void clear() {
        std::lock_guard<std::mutex> lock(mtx);
        cache_map.clear();
        access_count.clear();
    }

private:
    // 后台淘汰线程工作函数
    void eviction_worker() {
        while (!stop_thread) {
            std::unique_lock<std::mutex> lock(mtx);
            // 等待指定的淘汰间隔或停止信号
            cv.wait_for(lock, eviction_interval, [this]() { return stop_thread.load(); });

            if (stop_thread) {
                break;
            }

            if (cache_map.size() > max_size) {
                // 执行淘汰操作
                evict();
            }
        }
    }

    // 淘汰函数，删除最少访问的35%的元素
    void evict() {
        std::time_t start_time = std::time(nullptr);

        // 将访问计数与键配对并存入一个 vector
        std::vector<std::pair<Key, long>> access_vector(access_count.begin(), access_count.end());

        // 计算要删除的数量（删除35%的元素）
        size_t to_remove_count = static_cast<size_t>(access_vector.size() * 0.35);

        if (to_remove_count == 0 && !access_vector.empty()) {
            to_remove_count = 1;  // 至少删除一个元素
        }

        if (to_remove_count == 0) {
            // 没有需要删除的元素
            std::time_t end_time = std::time(nullptr);
            LOG(debug) << "Elapsed time: " << (end_time - start_time) << " seconds of " << this << std::endl;
            return;
        }

        // 使用 std::nth_element 找到第 to_remove_count 个最小的元素
        std::nth_element(access_vector.begin(), access_vector.begin() + to_remove_count, access_vector.end(),
                      [](const std::pair<Key, long>& a, const std::pair<Key, long>& b) {
                          return a.second < b.second;  // 按照访问计数升序排列
                      });

        // 遍历前 to_remove_count 元素并删除它们
        for (size_t i = 0; i < to_remove_count; ++i) {
            Key key_to_remove = access_vector[i].first;
            cache_map.erase(key_to_remove);        // 从缓存中删除
            access_count.erase(key_to_remove);    // 从访问计数中删除
        }

        std::time_t end_time = std::time(nullptr);
        LOG(debug) << "Elapsed time: " << (end_time - start_time) << " seconds of " << this << std::endl;
    }
};

template <typename Key, typename Value>
class LFUCache {
    // 定义一个结构体来存储值和对应的频率
    struct CacheEntry {
        Value value;
        size_t freq;
    };

    size_t max_size;
    size_t min_freq;

    // key 到 CacheEntry 的映射
    std::unordered_map<Key, CacheEntry> cache_map;

    // 频率到键列表的映射
    std::unordered_map<size_t, std::list<Key>> freq_map;

    // key 到其在 freq_map 中位置的映射，便于O(1)删除
    std::unordered_map<Key, typename std::list<Key>::iterator> key_iter_map;

public:
    LFUCache(size_t size) : max_size(size), min_freq(0) {}

    bool get(const Key& key, Value& value) {
        auto it = cache_map.find(key);
        if (it == cache_map.end()) {
            return false;
        }

        // 获取当前频率
        size_t freq = it->second.freq;

        // 更新频率
        freq_map[freq].erase(key_iter_map[key]);
        if (freq_map[freq].empty()) {
            freq_map.erase(freq);
            if (min_freq == freq) {
                min_freq++;
            }
        }

        // 增加频率
        it->second.freq++;
        freq_map[it->second.freq].push_back(key);
        key_iter_map[key] = --freq_map[it->second.freq].end();

        value = it->second.value;
        return true;
    }

    void put(const Key& key, const Value& value) {
        if (max_size == 0) return;

        auto it = cache_map.find(key);
        if (it != cache_map.end()) {
            // 更新值
            it->second.value = value;
            // 更新频率
            get(key, it->second.value);
            return;
        }

        if (cache_map.size() >= max_size) {
            // 找到最小频率
            auto key_to_evict = freq_map[min_freq].front();
            freq_map[min_freq].pop_front();
            if (freq_map[min_freq].empty()) {
                freq_map.erase(min_freq);
            }
            cache_map.erase(key_to_evict);
            key_iter_map.erase(key_to_evict);
        }

        // 插入新元素，频率为1
        cache_map[key] = CacheEntry{value, 1};
        freq_map[1].push_back(key);
        key_iter_map[key] = --freq_map[1].end();
        min_freq = 1;
    }

    void clear() {
        cache_map.clear();
        freq_map.clear();
        key_iter_map.clear();
        min_freq = 0;
    }

private:
    // 不再需要 evict 函数，因为淘汰逻辑已集成到 put 函数中
};

#include <unordered_map>
#include <list>
#include <mutex>
#include <thread>
#include <vector>
#include <functional>
#include <cmath>
#include <chrono>
#include <atomic>

// 单个分片的 LRU 缓存实现（包含后台淘汰功能）
template <typename Key, typename Value>
class SingleLRUCache {
public:
    SingleLRUCache(size_t capacity) 
        : max_size(capacity), stop_eviction(false) {}

    // 启动后台淘汰线程
    void start_eviction_thread(std::chrono::milliseconds interval) {
        eviction_thread = std::thread(&SingleLRUCache::eviction_worker, this, interval);
    }

    // 停止后台淘汰线程
    void stop_eviction_thread() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            stop_eviction = true;
        }
        cv.notify_all();
        if (eviction_thread.joinable()) {
            eviction_thread.join();
        }
    }

    // 获取值，如果存在则返回 true 并通过引用参数返回值
    bool get(const Key& key, Value& value) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = cache_map.find(key);
        if (it == cache_map.end()) {
            return false;
        }
        // 将访问的元素移动到链表头部，表示最近使用
        cache_list.splice(cache_list.begin(), cache_list, it->second);
        value = it->second->second;
        return true;
    }

    // 插入或更新缓存
    void put(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = cache_map.find(key);
        if (it != cache_map.end()) {
            // 更新已有元素的值并移动到链表头部
            it->second->second = value;
            cache_list.splice(cache_list.begin(), cache_list, it->second);
            return;
        }

        // 插入新元素到链表头部
        cache_list.emplace_front(key, value);
        cache_map[key] = cache_list.begin();

        // 如果缓存超过最大容量，移除链表尾部的最久未使用元素
        if (cache_map.size() > max_size) {
            auto last = cache_list.end();
            last--;
            cache_map.erase(last->first);
            cache_list.pop_back();
        }
    }

    // 清空缓存
    void clear() {
        std::lock_guard<std::mutex> lock(mtx);
        cache_map.clear();
        cache_list.clear();
    }

    ~SingleLRUCache() {
        stop_eviction_thread();
    }

private:
    size_t max_size;
    std::list<std::pair<Key, Value>> cache_list; // 双向链表，存储键值对
    std::unordered_map<Key, typename std::list<std::pair<Key, Value>>::iterator> cache_map; // 键到链表迭代器的映射
    std::mutex mtx; // 子缓存的互斥锁

    // 后台淘汰线程相关
    std::thread eviction_thread;
    std::condition_variable cv;
    std::mutex eviction_mtx;
    bool stop_eviction;

    // 后台淘汰线程工作函数
    void eviction_worker(std::chrono::milliseconds interval) {
        while (true) {
            std::unique_lock<std::mutex> lock(eviction_mtx);
            if (cv.wait_for(lock, interval, [this]() { return stop_eviction; })) {
                break;
            }
            perform_eviction();
        }
    }

    // 淘汰函数，删除最少访问的35%的元素（可根据需求调整）
    void perform_eviction() {
        std::lock_guard<std::mutex> lock(mtx);
        if (cache_map.size() <= max_size) {
            return;
        }

        size_t to_remove_count = static_cast<size_t>(std::ceil(cache_map.size() * 0.35));
        if (to_remove_count == 0 && !cache_map.empty()) {
            to_remove_count = 1; // 至少删除一个元素
        }

        // 创建一个临时 vector 来存储键值对
        std::vector<std::pair<Key, typename std::list<std::pair<Key, Value>>::iterator>> access_vector;
        access_vector.reserve(cache_map.size());
        for (auto it = cache_map.begin(); it != cache_map.end(); ++it) {
            access_vector.emplace_back(*it);
        }

        // 使用 std::nth_element 找到第 to_remove_count 个最小的元素
        std::nth_element(access_vector.begin(), access_vector.begin() + to_remove_count, access_vector.end(),
            [&](const std::pair<Key, typename std::list<std::pair<Key, Value>>::iterator>& a,
                const std::pair<Key, typename std::list<std::pair<Key, Value>>::iterator>& b) {
                // 这里我们可以根据需要定义排序逻辑，例如基于访问时间
                // 目前简单地按照列表位置（LRU）排序，最久未使用的在后
                // 如果需要基于其他指标（如访问计数），需要额外维护
                return a.second < b.second;
            });

        // 删除前 to_remove_count 个元素
        for (size_t i = 0; i < to_remove_count && i < access_vector.size(); ++i) {
            cache_map.erase(access_vector[i].first);
            cache_list.erase(access_vector[i].second);
        }
    }
};

#include <vector>
#include <functional>
#include <cmath>
#include <memory>

// 分片管理的 LRU 缓存实现（包含后台淘汰功能）
template <typename Key, typename Value, size_t NumShards = 16>
class ShardedLRUCache {
public:
    ShardedLRUCache(size_t total_capacity, std::chrono::milliseconds eviction_interval = std::chrono::milliseconds(5000))
        : total_capacity(total_capacity), eviction_interval(eviction_interval) {
        size_t shard_capacity = static_cast<size_t>(std::ceil(static_cast<double>(total_capacity) / NumShards));
        shards.reserve(NumShards);
        for (size_t i = 0; i < NumShards; ++i) {
            shards.emplace_back(new SingleLRUCache<Key, Value>(shard_capacity));
            shards.back()->start_eviction_thread(eviction_interval);
        }
    }

    ~ShardedLRUCache() {
        for (auto& shard : shards) {
            shard->stop_eviction_thread();
        }
    }

    // 获取值，如果存在则返回 true 并通过引用参数返回值
    bool get(const Key& key, Value& value) {
        size_t shard_idx = get_shard(key);
        return shards[shard_idx]->get(key, value);
    }

    // 插入或更新缓存
    void put(const Key& key, const Value& value) {
        size_t shard_idx = get_shard(key);
        shards[shard_idx]->put(key, value);
    }

    // 清空所有分片的缓存
    void clear() {
        for (auto& shard : shards) {
            shard->clear();
        }
    }

private:
    size_t total_capacity;
    std::vector<std::unique_ptr<SingleLRUCache<Key, Value>>> shards;
    std::hash<Key> hasher;
    std::chrono::milliseconds eviction_interval;

    // 根据键的哈希值确定所属的分片索引
    size_t get_shard(const Key& key) const {
        return hasher(key) % NumShards;
    }
};

class OPE {
 public:
    OPE(const std::string &keyarg, size_t plainbits, size_t cipherbits)
    : key(keyarg), pbits(plainbits), cbits(cipherbits), aesk(aeskey(key)),
    dgap_cache(get_cache_size_from_env("OPE_CACHE_SIZE", 10000)) {}

    NTL::ZZ encrypt(const NTL::ZZ &ptext);
    NTL::ZZ decrypt(const NTL::ZZ &ctext);

 private:
    static std::string aeskey(const std::string &key) {
        auto v = sha256::hash(key);
        v.resize(16);
        return v;
    }

    std::string key;
    size_t pbits, cbits;

    AES aesk;
    // std::map<NTL::ZZ, NTL::ZZ> dgap_cache;
    // LRUCache<NTL::ZZ, NTL::ZZ> dgap_cache;
    LFUCache<NTL::ZZ, NTL::ZZ> dgap_cache;

    template<class CB>
    ope_domain_range search(CB go_low);

    template<class CB>
    ope_domain_range lazy_sample(const NTL::ZZ &d_lo, const NTL::ZZ &d_hi,
                                 const NTL::ZZ &r_lo, const NTL::ZZ &r_hi,
                                 CB go_low, blockrng<AES> *prng);
};
