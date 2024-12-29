#include "log.h"
#include "common.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <thread>
// #include <sched.h>   todo:if set cpu affinity and cross-platform needed
#include "itoa.h"
namespace Argus
{

evil_logger evil_logger::instance_;
bool log_helper::is_running_{false};
char *log_helper::fragment_{nullptr};
std::thread log_helper::log_thread_;
time_spot log_helper::time_spot_;

void evil_logger::init_buffer()
{
    if (pbuffer_ == nullptr) {
        while (likely(lock_.test_and_set(std::memory_order_acquire)))
            ;
        pbuffer_ = &buffer;
        if (unlikely(!buffer_maintaner::useful(pbuffer_))) {
            buffer_maintaner::erase(pbuffer_);
        }
        lock_.clear();
        buffers_.push_back(pbuffer_);
        log_buffer_id_++;
    }
}
void evil_logger::register_loginfo(int &log_id, const log_info &info)
{
    if (unlikely(log_id != NAUGHTY_LOGID)) return;
    static int id = 0;
    while (likely(log_info_lock_.test_and_set(std::memory_order_acquire)))
        ;

    log_info_.emplace_back(info);
    log_info_lock_.clear();
    log_id = id++;
}

uint64_t log_helper::decode_msg(char *beg, uint64_t bytes, std::vector<log_info> &log_info, int thread_id)
{
    static auto &logger = evil_logger::instance();
    uint64_t remaining = bytes;
    char *cur_read_pos = beg;
    while (remaining > 0) {
        auto *header = (log_header *)cur_read_pos;
        if (unlikely(header->log_id >= log_info.size())) {
            // next turn
            break;
        }
        auto &info = log_info[header->log_id];
        // log by log version
        info.io_callback(logger.fd_, info, cur_read_pos, &fragment_, thread_id);
        cur_read_pos += header->data_len + sizeof(log_header);
        remaining -= header->data_len + sizeof(log_header);
    }
    return remaining;
}

int parse_header(char *time_header, std::tm *tm, double nanos, const char *filename, int filename_len, int log_level, int thread_id)
{
    static constexpr const char * const levels[] = {"] [ERROR] ", "] [WARNING] ", "] [INFO] ", "] [TRACE] ", "] [DEBUG]"};
    int y = tm->tm_year + 1900;
    int mon = tm->tm_mon + 1;
    int day = tm->tm_mday;
    int hour = tm->tm_hour;
    int min = tm->tm_min;
    int sec = tm->tm_sec;
    static std::once_flag flag;
    std::call_once(flag, [&] {
        time_header[0] = '[';
        time_header[5] = '-';
        time_header[8] = '-';
        time_header[11] = ' ';
        time_header[14] = ':';
        time_header[17] = ':';
        time_header[20] = '.';
        time_header[30] = ']';
        time_header[31] = ' ';
        time_header[32] = '[';
    });

    constexpr int ascii_offset = 48;
    time_header[1] = y / 1000 + ascii_offset;
    time_header[2] = (y / 100) % 10 + ascii_offset;
    time_header[3] = (y / 10) % 10 + ascii_offset;
    time_header[4] = (y % 10) + ascii_offset;

    time_header[6] = (mon / 10) + ascii_offset;
    time_header[7] = (mon % 10) + ascii_offset;

    time_header[9] = (day / 10) + ascii_offset;
    time_header[10] = (day % 10) + ascii_offset;

    time_header[12] = (hour / 10) + ascii_offset;
    time_header[13] = (hour % 10) + ascii_offset;

    time_header[15] = (min / 10) + ascii_offset;
    time_header[16] = (min % 10) + ascii_offset;

    time_header[18] = (sec / 10) + ascii_offset;
    time_header[19] = (sec % 10) + ascii_offset;
    long lnano = nanos;
    time_header[29] = (lnano % 10) + ascii_offset;
    time_header[28] = (lnano / 10) % 10 + ascii_offset;
    time_header[27] = (lnano / 100) % 10 + ascii_offset;
    time_header[26] = (lnano / 1000) % 10 + ascii_offset;
    time_header[25] = (lnano / 10000) % 10 + ascii_offset;
    time_header[24] = (lnano / 100000) % 10 + ascii_offset;
    time_header[23] = (lnano / 1000000) % 10 + ascii_offset;
    time_header[22] = (lnano / 10000000) % 10 + ascii_offset;
    time_header[21] = (lnano / 100000000) % 10 + ascii_offset;
    memcpy(time_header + 33, filename, filename_len);
    int cur_offset = 33 + filename_len;

    constexpr const char *trd_str = "] [Thread:";
    memcpy(time_header + cur_offset, trd_str, 10);
    cur_offset += 10;
    std::string_view sv = Argus::itoa10<char>(thread_id);
    int trd_width = strlen(sv.data());
    memcpy(time_header + cur_offset, sv.data(), trd_width);
    cur_offset += trd_width;

    switch (log_level) {
    case 0:
    case 3:
    case 4: {
        memcpy(time_header + cur_offset, levels[log_level], 10);
        cur_offset += 10;
        break;
    }
    case 1: {
        memcpy(time_header + cur_offset, levels[log_level], 12);
        cur_offset += 12;
        break;
    }
    case 2:
    default: {
        memcpy(time_header + cur_offset, levels[log_level], 9);
        cur_offset += 9;
        break;
    }
    }
    return cur_offset;
}

void log_helper::log_proc_main()
{
    // cpu_set_t cpu_set;
    // CPU_ZERO(&cpu_set);
    // CPU_SET(2, &cpu_set);
    // int ret = sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set);
    is_running_ = true;
    auto &logger = evil_logger::instance();
    std::vector<log_info> log_info_cpy; // trying to avoid lock in most times
    while (is_running_) {
        if (unlikely(logger.log_info_.size() != log_info_cpy.size())) {
            while (likely(logger.log_info_lock_.test_and_set(std::memory_order_acquire)))
                ;
            auto idx_beg = log_info_cpy.size();
            for (uint64_t idx = idx_beg; idx < logger.log_info_.size(); ++idx) log_info_cpy.push_back(logger.log_info_[idx]);
            logger.log_info_lock_.clear();
        }
        auto buf_cap = logger.log_buffer_id_.load();
        for (uint64_t i = 0; i < buf_cap; ++i) {
            while (unlikely(logger.lock_.test_and_set(std::memory_order_acquire)))
                ;
            circle_buffer *buf = logger.buffers_[i];
            logger.lock_.clear();
            if (!buffer_maintaner::useful(buf)) continue;
            uint64_t peeked_bytes{0};
            char *read_pos = buf->peek(&peeked_bytes);
            if (peeked_bytes > 0) {
#ifndef FAKELOG
                auto left = decode_msg(read_pos, peeked_bytes, log_info_cpy, (int)i + 1);
                buf->consume(peeked_bytes - left);
#else
                buf->consume(peeked_bytes);
#endif
            }
        }
    }
}

void log_helper::init_fragment()
{
    fragment_ = (char *)malloc(FRAGMENT_SIZE);
}

void log_helper::flush()
{
    static auto &logger = evil_logger::instance();
    while (logger.has_pending_data())
        ;
#ifdef ASYNC_MODE
    static char *write_buffer = evil_logger::instance().get_write_buffer();
    static uint64_t &waterline = evil_logger::instance().get_waterline();
    fwrite(write_buffer, waterline, 1, logger.fd_);
#endif
    fflush(logger.fd_);
}

void log_helper::stop()
{
    static auto &logger = evil_logger::instance();
    while (logger.has_pending_data()) {}

    is_running_ = false;

    free(fragment_);
    log_thread_.join();
#ifdef ASYNC_MODE
    static char *write_buffer = evil_logger::instance().get_write_buffer();
    static uint64_t &waterline = evil_logger::instance().get_waterline();
    fwrite(write_buffer, waterline, 1, evil_logger::instance().fd_);
#endif
    fclose(evil_logger::instance().fd_);
}

void log_helper::track_init_time()
{
    time_spot_.rdtsc = Argus::cycles::rdtsc();
    time_spot_.unixtime = std::time(nullptr);
    time_spot_.cycles_persecond = Argus::cycles::get_cycles_persec();
}

class logger_wrapper
{
public:
    logger_wrapper()
    {
        log_helper::track_init_time();
    }
    ~logger_wrapper()
    {
        log_helper::stop();
    }
};

void set_log_path(const char *path)
{
    evil_logger::instance().set_log_path(path);
    log_helper::init_fragment();
    log_helper::log_thread_ = std::thread(&log_helper::log_proc_main);
    static logger_wrapper wrapper;
}

void sync_log()
{
    log_helper::flush();
}

} // namespace Argus