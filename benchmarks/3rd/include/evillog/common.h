#pragma once
#include <atomic>
#include <array>
#include <cstdio>

#ifndef _WIN32
#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif
#define NAUGHTY_LOGID -1
namespace Argus
{
constexpr int BUFFER_SIZE = 1 << 20;
constexpr int CACHELINE_PADDING_SIZE = 64;
constexpr uint32_t FRAGMENT_SIZE = 1 << 16;
constexpr uint64_t WRITE_CAP = 1 << 27;

enum PARAM_TYPE : int8_t
{
    STRING = 0,
    FIXED,
    INT,
    INVALID, // not supported
};

enum LOG_LEVEL : int8_t
{
    ERROR = 0,
    WARN,
    INFO,
    TRACE,
    DEBUG,
};

struct log_header
{
    uint32_t log_id;
    uint32_t data_len;
    uint64_t timestamp;
    char arg_data[];
};

struct log_info
{
    using write_fn = void (*)(FILE *, const log_info &, char *, char **, int);

    constexpr log_info(const char *filename,
                       const uint32_t linenum,
                       const uint8_t log_level,
                       const char *fmt_str,
                       const int filename_len,
                       const PARAM_TYPE *param_types,
                       const int fmt_len,
                       write_fn io_cb) :
        filename(filename),
        linenum(linenum),
        log_level(log_level),
        fmt_str(fmt_str),
        filename_len(filename_len),
        param_types(param_types),
        fmt_len(fmt_len),
        io_callback(io_cb)
    {}
    const char *filename;
    const uint32_t linenum;
    const uint8_t log_level;
    const char *fmt_str;
    int filename_len;
    const PARAM_TYPE *param_types;
    const int fmt_len;
    write_fn io_callback;
};

inline void lfence()
{
#if defined(__x86_64__) || defined(__i386__)
#ifdef __linux__
    asm volatile("lfence" ::
                     : "memory");
#elif defined(__WIN32)
    __asm volatile lfence;
#endif
#else
    std::atomic_thread_fence(std::memory_order_acquire);
#endif
}

inline void sfence()
{
#if defined(__x86_64__) || defined(__i386__)
#ifdef __linux__
    __asm__ __volatile__("sfence" ::
                             : "memory");
#elif defined(_WIN32)
    __asm volatile lfence;
#endif
#else
    std::atomic_thread_fence(std::memory_order_release);
#endif
}

void set_log_path(const char *path);

void sync_log();

} // namespace Argus
