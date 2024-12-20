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

#include <iostream>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <ctime>

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

public:
    LRUCache(size_t size) : max_size(size) {
        // LOG(debug) << "max_size of LRUCache = " << max_size << " of " << this;
    }

    bool get(const Key& key, Value& value) {
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

        // 如果缓存超出最大容量，删除最少访问的 35% 元素
        if (cache_map.size() > max_size) {
            evict();
        }
    }

    // 非安全插入，不做检查直接插入，默认 key 不存在
    void put(const Key& key, const Value& value) {
        // 插入或更新缓存
        cache_map[key] = value;
        access_count[key] = 1;  // 新插入元素的访问计数为1

        // 如果缓存超出最大容量，删除最少访问的 35% 元素
        if (cache_map.size() > max_size) {
            evict();
        }
    }

    void clear() {
        cache_map.clear();
        access_count.clear();
    }

private:
    void evict() {
        // std::time_t start_time = std::time(nullptr);

        // 将访问计数与键配对并存入一个 vector
        std::vector<std::pair<Key, long>> access_vector(access_count.begin(), access_count.end());

        // 计算要删除的数量（删除 35% 的元素）
        size_t to_remove_count = static_cast<size_t>(access_vector.size() * 0.35);

        if (to_remove_count == 0 && !access_vector.empty()) {
            to_remove_count = 1;  // 至少删除一个元素
        }

        if (to_remove_count == 0) {
            // 没有需要删除的元素
            // std::time_t end_time = std::time(nullptr);
            // LOG(debug) << "Elapsed time: " << (end_time - start_time) << " seconds of " << this;
            return;
        }

        // 使用 std::nth_element 找到第 to_remove_count 个最小的元素
        std::nth_element(access_vector.begin(), access_vector.begin() + to_remove_count, access_vector.end(),
                      [](const std::pair<Key, long>& a, const std::pair<Key, long>& b) {
                          return a.second < b.second;  // 按照访问计数升序排列
                      });

        // 只保留前 to_remove_count 个最少访问的元素
        // 由于 std::nth_element 只是部分排序，前 to_remove_count 元素是最小的，但顺序不确定
        // 所以遍历前 to_remove_count 元素并删除它们
        for (size_t i = 0; i < to_remove_count; ++i) {
            Key key_to_remove = access_vector[i].first;
            cache_map.erase(key_to_remove);        // 从缓存中删除
            access_count.erase(key_to_remove);    // 从访问计数中删除
        }

        // // 获取结束时间
        // std::time_t end_time = std::time(nullptr);
        // // 计算并输出时间差（秒）
        // LOG(debug) << "Elapsed time: " << (end_time - start_time) << " seconds of " << this;
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
    LRUCache<NTL::ZZ, NTL::ZZ> dgap_cache;

    template<class CB>
    ope_domain_range search(CB go_low);

    template<class CB>
    ope_domain_range lazy_sample(const NTL::ZZ &d_lo, const NTL::ZZ &d_hi,
                                 const NTL::ZZ &r_lo, const NTL::ZZ &r_hi,
                                 CB go_low, blockrng<AES> *prng);
};
