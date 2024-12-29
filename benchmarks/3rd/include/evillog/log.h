#pragma once
#include "format.h"
#include <ctime>
#include <mutex>
#include <thread>
#include "msg_protocol.h"
#include "time_util.h"
#include <unordered_set>
#define ASYNC_MODE

namespace Argus
{

int parse_header(char *time_header, std::tm *tm, double nanos = 0.0, const char *filename = "", int filename_len = 0, int log_level = 0, int thread_id = 0);
class buffer_maintaner;
class circle_buffer
{
    //Considering data prefetch with 2 canche line size on intel x86
    using cache_line_pad_t = char[2 * CACHELINE_PADDING_SIZE];

public:
    ~circle_buffer();
    circle_buffer(uint64_t id) :
        id_(id)
    {}

    inline char *alloc(unsigned nbytes)
    {
        if (likely(nbytes < available_bytes_)) return procucer_pos_;

        return busy_alloc(nbytes);
    }

    char *busy_alloc(unsigned nbytes);

    inline void finish_alloc(unsigned nbytes)
    {
        sfence(); // Ensures producer finishes writes before bump
        available_bytes_ -= nbytes;
        procucer_pos_ += nbytes;
    }

    void consume(unsigned nbytes)
    {
        lfence(); // Make sure consumer reads finish before bump
        consumer_pos_ += nbytes;
    }

    char *peek(uint64_t *bytes_available);
#ifdef DEBUG_MODE
    void show()
    {
        printf("c_pos:%p, p_pos:%p,available_bytes_:%lu\n", consumer_pos_, procucer_pos_, available_bytes_);
    }
#endif
    bool empty()
    {
        return consumer_pos_ == procucer_pos_;
    }

private:
    uint64_t id_;
    char buff_[BUFFER_SIZE]{'\0'};
    char *volatile procucer_pos_{buff_};
    char *real_buff_end_pos_{buff_ + BUFFER_SIZE};
    uint64_t available_bytes_{BUFFER_SIZE};
    cache_line_pad_t pad_;
    char *volatile consumer_pos_{buff_};
};

class buffer_maintaner
{
public:
    static bool useful(void *addr);
    static void maintan(void *addr);
    static void erase(void *addr);

private:
    static std::unordered_set<uint64_t> judge_book_;
    static std::atomic_flag lock_;
};
struct log_header;
class evil_logger
{
    friend class log_helper;

public:
    bool has_pending_data()
    {
        for (auto &buffer : buffers_) {
            if (buffer_maintaner::useful(buffer) && !buffer->empty()) {
                return true;
            }
        }
        return false;
    }
    void set_log_path(const char *path)
    {
        strcpy(log_path, path);
        fd_ = fopen(path, "a");
    }
    void set_loglevel(LOG_LEVEL level) { loglevel_ = level; }

    void register_loginfo(int &log_id, const log_info &info);

    void init_buffer();

    static inline char *beg_alloc(size_t nbytes)
    {
        if (unlikely(pbuffer_ == nullptr)) {
            instance_.init_buffer();
        }
        return pbuffer_->alloc(nbytes);
    }

    static inline void end_alloc(size_t nbytes) { pbuffer_->finish_alloc(nbytes); }

    static evil_logger &instance() { return instance_; }

    uint64_t &get_waterline() { return waterline_; }

    char *get_write_buffer() { return (char *)write_buffer_; }

private:
    static evil_logger instance_;
    static thread_local circle_buffer *pbuffer_;
    static thread_local circle_buffer buffer;
    std::vector<circle_buffer *> buffers_;
    std::vector<log_info> log_info_;
    char log_path[512]{'\0'};
    LOG_LEVEL loglevel_{LOG_LEVEL::INFO};
    volatile std::atomic_uint64_t log_buffer_id_{0};
    FILE *fd_;
    std::atomic_flag lock_{ATOMIC_FLAG_INIT};
    std::atomic_flag log_info_lock_{ATOMIC_FLAG_INIT};
    char *write_buffer_[WRITE_CAP];
    uint64_t waterline_{0};
};

class log_helper
{
public:
    static void track_init_time();
    static void flush();
    static LOG_LEVEL get_log_level() { return evil_logger::instance_.loglevel_; }
    static void init_fragment();
    static void log_proc_main();
    static void stop();
    static uint64_t decode_msg(char *beg, uint64_t bytes, std::vector<log_info> &log_info, int);

public:
    static bool is_running_;
    static time_spot time_spot_;
    static char *fragment_;
    static std::thread log_thread_;
};

template <typename... Ts>
inline void dump_real(FILE *fd, const log_info &info, char *buffer_start, char **fragment, int thread_id)
{
#ifdef ASYNC_MODE
    static char *write_buffer = evil_logger::instance().get_write_buffer();
    static uint64_t &waterline = evil_logger::instance().get_waterline();
#endif
    static std::vector<char *> reverse_params;
    static uint32_t fragment_size{FRAGMENT_SIZE};
    auto *header = (log_header *)buffer_start;
    buffer_start += sizeof(log_header);
    char *arg_data = buffer_start;
    uint32_t buf_written_len = 0;
    while (!likely(reverse_from_buffer<Ts...>(info, &arg_data, reverse_params, fragment, FRAGMENT_SIZE, buf_written_len))) {
        reverse_params.clear();
        // resize and redo
        arg_data = buffer_start;
        buf_written_len = 0;
        fragment_size = fragment_size * 2;
        free(*fragment);
        *fragment = (char *)malloc(fragment_size);
        memset(*fragment, '\0', fragment_size);
    }
    // hanldle log header
    auto secondsSinceCheckpoint = Argus::cycles::to_seconds(
        header->timestamp - log_helper::time_spot_.rdtsc,
        log_helper::time_spot_.cycles_persecond);
    int64_t whole_seconds = static_cast<int64_t>(secondsSinceCheckpoint);
    auto nanos = 1.0e9 * (secondsSinceCheckpoint - static_cast<double>(whole_seconds));

    std::time_t abs_time = whole_seconds + log_helper::time_spot_.unixtime;
    std::tm *tm = gmtime(&abs_time);
    static int timezone = get_current_timezone();
    tm->tm_hour += timezone;
    static char line[2048]{'\0'};
    int calc_len = parse_header(line, tm, nanos, info.filename, info.filename_len, info.log_level, thread_id);
#ifdef ASYNC_MODE
    int idx{0};
    sprintf(line + calc_len, info.fmt_str, decode_param<Ts>(idx, sizeof...(Ts), info, reverse_params)...);
    calc_len = strlen(line);
    if (waterline + calc_len > WRITE_CAP) {
        fwrite(write_buffer, waterline, 1, fd);
        waterline = 0;
    };
    memcpy(write_buffer + waterline, line, calc_len);
    waterline += calc_len;
#else
    fprintf(fd, "[%s.%09.0lf] [%s:%u] [Thread:%u] [%s]: ", time_string, nanos, info.filename, info.linenum, thread_id, levels[info.log_level]);
    fprintf(fd, info.fmt_str, decode_param<Ts>(idx, sizeof...(Ts), info, reverse_params)...);
#endif
    reverse_params.clear();
}

template <typename... Ts>
inline void write_cb(FILE *fd, const log_info &info, char *beg, char **fragment, int thread_id)
{
    dump_real<Ts...>(fd, info, beg, fragment, thread_id);
}

constexpr const char *relative_fn(const char *full_name)
{
    int last_idx = 0;
    int cur_idx = 0;
    while (true) {
        if (*(full_name + cur_idx) == '\\' || *(full_name + cur_idx) == '/') {
            last_idx = cur_idx + 1;
        }
        if (*(full_name + cur_idx) == '\0') break;
        ++cur_idx;
    }
    return full_name + last_idx;
}

template <long unsigned int N, int M, typename... Ts>
inline void
log(int &logId,
    const char *filename,
    const int linenum,
    const LOG_LEVEL log_level,
    const char (&format)[M],
    const int filename_len,
    const std::array<PARAM_TYPE, N> &param_types,
    Ts... args)
{
    static_assert(sizeof...(args) == N, "log params mismatch!!!");
    if (unlikely(logId == NAUGHTY_LOGID)) {
        const PARAM_TYPE *array = param_types.data();
        log_info info(filename, linenum, log_level, format, filename_len, array, M, &write_cb<Ts...>);
        evil_logger::instance().register_loginfo(logId, info);
    }
    auto tv = Argus::cycles::rdtsc();
    size_t arg_sizes[N + 1] = {};
    size_t allocated_size = calc_arg_sizes(param_types, arg_sizes, args...) + sizeof(log_header);

    char *write_pos = evil_logger::beg_alloc(allocated_size);
#ifdef _WIN32
    log_header *header = new (write_pos) log_header{(uint32_t)logId, (uint32_t)(allocated_size - sizeof(log_header)), tv, {0}};
#else
    log_header *header = new (write_pos) log_header{(uint32_t)logId, (uint32_t)(allocated_size - sizeof(log_header)), tv, {}};
#endif
    (void)header;
    write_pos += sizeof(log_header);
    encode_arguments(param_types, arg_sizes, &write_pos, args...);
    evil_logger::end_alloc(allocated_size);
}

template <size_t N>
constexpr std::array<Argus::PARAM_TYPE, N> reverse_array(const std::array<Argus::PARAM_TYPE, N> &in)
{
    std::array<Argus::PARAM_TYPE, N> out{};
    for (size_t i = 0; i < N; i++) {
        out[i] = in[N - i - 1];
    }
    return out;
}

constexpr int strlen_cst(const char *full_name)
{
    int cur_idx = 0;
    for (; cur_idx < 1024; ++cur_idx) {
        if (*(full_name + cur_idx) == '\0') break;
    }
    return cur_idx;
}

#define EVIL_LOG(severity, format, ...)                                                                                 \
    do {                                                                                                                \
        if (Argus::severity > Argus::log_helper::get_log_level()) break;                                                \
        constexpr int param_num = Argus::count_fmt_params(format);                                                      \
        static constexpr std::array<Argus::PARAM_TYPE, param_num> param_types =                                         \
            Argus::fmtstr2cst_array<param_num>(format);                                                                 \
        static constexpr auto fmtln = Argus::string_concat(Argus::string_literal(format), Argus::string_literal("\n")); \
        static int logId = NAUGHTY_LOGID;                                                                               \
        constexpr const char *real_name = Argus::relative_fn(__FILE__);                                                 \
        constexpr auto cst_len_real_name = Argus::strlen_cst(real_name);                                                \
        Argus::log(logId, real_name, __LINE__, Argus::severity, fmtln.s,                                                \
                   cst_len_real_name, param_types, ##__VA_ARGS__);                                                      \
    } while (0)

} // namespace Argus
