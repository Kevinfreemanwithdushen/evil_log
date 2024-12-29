#include <cstdio>
#include <cstdlib>
#include <ctime>
// #include "NanoLogCpp17.h"
#include <random>
#include <chrono>
#include <string>
#include <string_view>
#include "common.h"
#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "evillog/log.h"
#include <getopt.h>
using namespace std;
// using namespace NanoLog::LogLevels;

int multipy = 20;
class scope_timer
{
public:
    scope_timer(std::string_view sv, int times) :
        start_(std::chrono::high_resolution_clock::now()), name_(sv), times_(times)
    {
    }
    ~scope_timer()
    {
        stop_ = std::chrono::high_resolution_clock::now();
        sp_ = std::chrono::duration_cast<std::chrono::duration<double>>(
                  stop_ - start_)
                  .count();
        nanos_ += (sp_ / times_) * 1e9;
        printf("%s took %0.2lf seconds (%0.2lf ns/message average)\n",
               name_.data(), sp_, (sp_ / times_) * 1e9);
    }
    static double nanos()
    {
        return nanos_;
    }

private:
    std::chrono::high_resolution_clock::time_point start_;
    std::chrono::high_resolution_clock::time_point stop_;
    std::string_view name_;
    int times_;
    static double nanos_;
    std::chrono::duration<double>::rep sp_;
};

double scope_timer::nanos_ = 0.0f;

class scope_time
{
public:
    scope_time(std::string_view sv, int times) :
        start_(std::chrono::high_resolution_clock::now()), name_(sv), times_(times)
    {}
    ~scope_time()
    {
        stop_ = std::chrono::high_resolution_clock::now();
        auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(
                             stop_ - start_)
                             .count();

        printf("%s took %0.2lf seconds (%0.2lf ns/message average)\n",
               name_.data(), time_span, (time_span / times_) * 1e9);
    }

private:
    std::chrono::high_resolution_clock::time_point start_;
    std::chrono::high_resolution_clock::time_point stop_;
    std::string_view name_;
    int times_;
};


void hell4nanolog(int RECORDS, int log_type = 0)
{
    NanoLog::setLogFile("./tom.log");
    NanoLog::preallocate();
    NanoLog::setLogLevel(DEBUG);
    {
        scope_time timer{"0 parameters Nanolog", RECORDS};
        for (unsigned i = 0; i < RECORDS; ++i) {
            NANO_LOG(NOTICE, "Simple log message with 0 parameter");
        }
    }
    NanoLog::sync();
}
void hell4evillog(int RECORDS, int log_type = 0)
{
    Argus::set_log_path("./test.log");
    {
        scope_time timer{"0 parameters evillog", RECORDS};
        for (unsigned i = 0; i < RECORDS; ++i) {
            EVIL_LOG(INFO, "Simple log message with 0 parameter");
        }
    }
}

void hell4spdlog(int RECORDS, int log_type = 0)
{
    auto async_file = spdlog::basic_logger_mt<spdlog::async_factory>("async_file_logger", "./async_log.txt");
    {
        scope_time timer{"0 parameters spdlog", RECORDS};
        for (unsigned i = 0; i < RECORDS; ++i) {
            async_file->log(spdlog::level::info, "Simple log message with 0 parameter");
        }
    }
}

void benchmakrk4nanolog(int RECORDS, int log_type = 0, int trd_num = std::thread::hardware_concurrency() - 1)
{
    NanoLog::setLogFile("./tom.log");
    NanoLog::preallocate();
    NanoLog::setLogLevel(DEBUG);
    std::vector<std::thread> thrds;
    for (int i = 0; i < trd_num; ++i) {
        if (log_type == 0) {
            thrds.emplace_back(std::thread([&] {
                for (int i = 0; i < multipy; ++i) {
                    {
                        scope_timer timer{"simple parameters nanolog", RECORDS};
                        for (int i = 0; i < (RECORDS); ++i) {
                             NANO_LOG(NOTICE, "Simple log message with 0 parameters");
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                printf("thread exit...\n");
            }));
        } else {
            thrds.emplace_back(std::thread([&] {
                char *s = new char[32]{'\0'};
                strncpy(s, "tom is a cat cccccccattttt", 31);
                for (int i = 0; i < multipy; ++i) {
                    {
                        scope_timer timer{"complexed parameters nanolog", RECORDS};
                        for (int i = 0; i < (RECORDS); ++i) {
                             NANO_LOG(NOTICE, "integer_test:%lu,integer_test2:%lu,,f1:%f str_test:%s, float_test:%f, str_test2:%s, emmmm:%d, str3333:%s", i + 2, 3.1415926,
                                     i, "tommy",
                                     3.67, s, 87618, "contry road take me home");
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
                printf("thread exit...\n");
            }));
        }
    }
    for (auto &trd : thrds) {
        trd.join();
    }
    auto nanos = scope_timer::nanos();
    printf("nanolog takes (%0.2lf ns/message average)\n", nanos / ((trd_num - 1) * (multipy)));
    NanoLog::sync();
}

void benchmakrk4evillog(int RECORDS, int log_type = 0, int trd_num = std::thread::hardware_concurrency() - 1)
{
    Argus::set_log_path("./test.log");
    std::vector<std::thread> thrds;
    for (int i = 0; i < trd_num; ++i) {
        if (log_type == 0) {
            thrds.emplace_back(std::thread([&] {
                for (int i = 0; i < multipy; ++i) {
                    {
                        scope_timer timer{"simple parameters evillog", RECORDS};
                        for (int i = 0; i < (RECORDS); ++i) {
                            EVIL_LOG(INFO, "Simple log message with 0 parameters");
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                printf("thread exit...\n");
            }));
        } else {
            thrds.emplace_back(std::thread([&] {
                char *s = new char[32]{'\0'};
                strncpy(s, "tom is a cat cccccccattttt", 31);
                for (int i = 0; i < multipy; ++i) {
                    {
                        scope_timer timer{"complexed parameters evillog", RECORDS};
                        for (int i = 0; i < (RECORDS); ++i) {
                            EVIL_LOG(INFO, "integer_test:%lu,integer_test2:%lu,,f1:%f str_test:%s, float_test:%f, str_test2:%s, emmmm:%d, str3333:%s", i + 2, 
                                     i,3.1415926, "tommy",
                                     3.67, s, 87618, "contry road take me home");
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
                printf("thread exit...\n");
            }));
        }
    }
    for (auto &trd : thrds) {
        trd.join();
    }
    auto nanos = scope_timer::nanos();
    printf("evillog takes (%0.2lf ns/message average)\n", nanos / ((trd_num - 1) * (multipy)));
}

void benchmakrk4spdlog(int RECORDS, int log_type = 0, int trd_num = std::thread::hardware_concurrency() - 1)
{
    auto async_file = spdlog::basic_logger_mt<spdlog::async_factory>("async_file_logger", "logs/async_log.txt");
std::vector<std::thread> thrds;
    for (int i = 0; i < trd_num; ++i) {
        if (log_type == 0) {
            thrds.emplace_back(std::thread([&] {
                for (int i = 0; i < multipy; ++i) {
                    {
                        scope_timer timer{"simple parameters spdlog", RECORDS};
                        for (int i = 0; i < (RECORDS); ++i) {
                            async_file->log(spdlog::level::info, "Simple log message with 0 parameters");
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                printf("thread exit...\n");
            }));
        } else {
            thrds.emplace_back(std::thread([&] {
                char *s = new char[32]{'\0'};
                strncpy(s, "tom is a cat cccccccattttt", 31);
                for (int i = 0; i < multipy; ++i) {
                    {
                        scope_timer timer{"complexed parameters spdlog", RECORDS};
                        for (int i = 0; i < (RECORDS); ++i) {
                             async_file->log(spdlog::level::info, "integer_test:{},integer_test2:{},,f1:{} str_test:{}, float_test:{}, str_test2:{}, emmmm:{}, str3333:{}", i + 2, 
                                     i,3.1415926, "tommy",
                                     3.67, s, 87618, "contry road take me home");
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
                printf("thread exit...\n");
            }));
        }
    }
    for (auto &trd : thrds) {
        trd.join();
    }
    auto nanos = scope_timer::nanos();
    printf("spdlog takes (%0.2lf ns/message average)\n", nanos / ((trd_num - 1) * (multipy)));
}

void run_benchmark(int log_type)
{
#ifdef BECH_SPDLOG
    benchmakrk4spdlog(5000, log_type);
#elif defined BECH_NANO
    benchmakrk4nanolog(5000, log_type);
#else
    benchmakrk4evillog(5000, log_type);
#endif
}

void hell_the_log(int log_type)
{
#ifdef BECH_SPDLOG
    hell4spdlog(10000000, log_type);
#elif defined BECH_NANO
    hell4nanolog(10000000, log_type);
#else
    hell4evillog(10000000, log_type);
#endif
}

void help(char *bin)
{
    printf("Usage: %s [OPTION...]\n", bin);
    printf("-m, mode[0:regular perf,1:hell mode for tesing throughput,");
    printf("2:regular perf of complex lodg, 3:hell mode for tesing throughput of complex log]\n");
    printf("-h, help info\n");
    printf("example: ./argus -m 0\n");
    std::exit(-1);
}

int main(int argc, char **argv)
{
    if (argc == 1) help(argv[0]);
    int opt;
    int mode = 0;
    while ((opt = getopt(argc, argv, "hm:")) != -1) {
        switch (opt) {
        case 'm':
            mode = std::atoi(optarg);
            break;
        case 'h':
        default:
            help(argv[0]);
        }
    }
    switch (mode) {
    case 0: {
        run_benchmark(0);
        break;
    }
    case 1: {
        hell_the_log(0);
        break;
    }
    case 2: {
        run_benchmark(1);
        break;
    }
    case 3: {
        hell_the_log(1);
        break;
    }
    default: {
        help(argv[0]);
    }
    }
    return 0;
}