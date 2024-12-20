#pragma once

#include <iostream>
#include <sstream>
#include <map>
#include <string>
#include <time.h>

#define LOG_GROUPS(m)       \
    m(warn)                 \
    m(debug)                \
    m(error)                \
    m(cdb_v)                \
    m(crypto)               \
    m(crypto_v)             \
    m(crypto_data)          \
    m(edb)                  \
    m(edb_v)                \
    m(edb_query)            \
    m(edb_query_plain)      \
    m(edb_perf)             \
    m(encl)                 \
    m(test)                 \
    m(am)                   \
    m(am_v)                 \
    m(mp)                   \
    m(wrapper)              \
    m(all)

enum class log_group {
#define __temp_m(n) log_ ## n,
LOG_GROUPS(__temp_m)
#undef __temp_m
};

// static
// std::map<std::string, log_group> log_name_to_group = {
// #define __temp_m(n) { #n, log_group::log_ ## n },
// LOG_GROUPS(__temp_m)
// #undef __temp_m
// };

static
std::map<log_group, std::string> log_group_to_name = {
#define __temp_m(n) { log_group::log_ ## n, #n },
LOG_GROUPS(__temp_m)
#undef __temp_m
};

class cryptdb_logger : public std::stringstream {
 public:
    cryptdb_logger(log_group g, const char *filearg, uint linearg, const char *fnarg)
        : m(mask(g)), file(filearg), line(linearg), func(fnarg), log_level(g)
    {
    }

    ~cryptdb_logger()
    {

        if (enable_mask & m) {
            
            time_t t;  //秒时间
            tm* local; //本地时间 
            char buf[128]= {0};
            
            t = time(NULL); //获取目前秒时间
            local = localtime(&t); //转为本地时间
            strftime(buf, 64, "%Y-%m-%d %H:%M:%S", local);
            
            // std::stringstream temp_stream;
            // temp_stream << str();
            // Output the log entry with current date and time
            std::cout << buf << " - " // Use put_time to format the datetime
                      << file << ":" << line << " (" << func << "): " 
                      << "[" << log_group_to_name[log_level] << "] " << str() << std::endl;
        }

    }

    static void
    enable(log_group g)
    {
        if (g == log_group::log_all)
            enable_mask = ~0ULL;
        else
            enable_mask |= mask(g);
    }

    static void
    disable(log_group g)
    {
        if (g == log_group::log_all)
            enable_mask = 0;
        else
            enable_mask &= ~mask(g);
    }

    static bool
    enabled(log_group g)
    {
        return enable_mask & mask(g);
    }

    static uint64_t
    mask(log_group g)
    {
        return 1ULL << ((int) g);
    }

    static std::string
    getConf()
    {
        std::stringstream ss;
        ss << enable_mask;
        return ss.str();
    }

    static void
    setConf(std::string conf)
    {
        std::stringstream ss(conf);
        ss >> enable_mask;
    }

 private:
    uint64_t m;
    const char *file;
    uint line;
    const char *func;
    log_group log_level; // 添加log_level成员变量

    static uint64_t enable_mask;

};

#define LOG(g) \
    (cryptdb_logger(log_group::log_ ## g, __FILE__, __LINE__, __func__))

