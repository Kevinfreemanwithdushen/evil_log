#include <errno.h>
#include <cstring>
#include <cstdio>
#include <exception>
#include "time_util.h"
#include <ctime>

namespace Argus
{

#ifdef _WIN32
int gettimeofday(struct timeval *tp, void *tzp)
{
  time_t clock;
  struct tm tm;
  SYSTEMTIME wtm;
  GetLocalTime(&wtm);
  tm.tm_year   = wtm.wYear - 1900;
  tm.tm_mon   = wtm.wMonth - 1;
  tm.tm_mday   = wtm.wDay;
  tm.tm_hour   = wtm.wHour;
  tm.tm_min   = wtm.wMinute;
  tm.tm_sec   = wtm.wSecond;
  tm. tm_isdst  = -1;
  clock = mktime(&tm);
  tp->tv_sec = clock;
  tp->tv_usec = wtm.wMilliseconds * 1000;
  return (0);
}
#endif


static int lc_time_zone = 0;

#define PERFUTILS_DIE(format_, ...)                     \
    do {                                                \
        fprintf(stderr, format_, ##__VA_ARGS__);        \
        fprintf(stderr, "%s:%d\n", __FILE__, __LINE__); \
        std::terminate();                               \
    } while (0)

class Initialize
{
public:
    /**
     * This form of constructor causes its function argument to be invoked
     * when the object is constructed.  When used with a static Initialize
     * object, this will cause #func to run before main() runs, so that
     * #func can perform once-only initialization.
     *
     * \param func
     *      This function is invoked with no arguments when the object is
     *      constructed.  Typically the function will create static
     *      objects and/or invoke other initialization functions.  The
     *      function should normally contain an internal guard so that it
     *      only performs its initialization the first time it is invoked.
     */
    Initialize(void (*func)())
    {
        (*func)();
    }

    /**
     * This form of constructor causes a new object of a particular class
     * to be constructed with a no-argument constructor and assigned to a
     * given pointer.  This form is typically used with a static Initialize
     * object: the result is that the object will be created and assigned
     * to the pointer before main() runs.
     *
     * \param p
     *      Pointer to an object of any type. If the pointer is NULL then
     *      it is replaced with a pointer to a newly allocated object of
     *      the given type.
     */
    template <typename T>
    explicit Initialize(T *&p)
    {
        if (p == NULL) {
            p = new T;
        }
    }
};

double cycles::cycles_persec = 0;
static Initialize _(cycles::init);

/**
 * Perform once-only overall initialization for the Cycles class, such
 * as calibrating the clock frequency.  This method is invoked automatically
 * during initialization, but it may be invoked explicitly by other modules
 * to ensure that initialization occurs before those modules initialize
 * themselves.
 */
void cycles::init()
{
    time_t time_utc = 0;
    struct tm *p_tm;
    p_tm = localtime(&time_utc);
    lc_time_zone = (p_tm->tm_hour > 12) ? (p_tm->tm_hour -= 24) : p_tm->tm_hour;
    if (cycles_persec != 0)
        return;

    // Compute the frequency of the fine-grained CPU timer: to do this,
    // take parallel time readings using both rdtsc and gettimeofday.
    // After 10ms have elapsed, take the ratio between these readings.

    struct timeval startTime, stopTime;
    uint64_t startCycles, stopCycles, micros;
    double oldCycles;

    // There is one tricky aspect, which is that we could get interrupted
    // between calling gettimeofday and reading the cycle counter, in which
    // case we won't have corresponding readings.  To handle this (unlikely)
    // case, compute the overall result repeatedly, and wait until we get
    // two successive calculations that are within 0.001% of each other (or
    // in other words, a drift of up to 10µs per second).
    oldCycles = 0;
    while (1) {
        if (gettimeofday(&startTime, NULL) != 0) {
            PERFUTILS_DIE("Cycles::init couldn't read clock: %s", strerror(errno));
        }
        startCycles = rdtsc();
        while (1) {
            if (gettimeofday(&stopTime, NULL) != 0) {
                PERFUTILS_DIE("Cycles::init couldn't read clock: %s",
                              strerror(errno));
            }
            stopCycles = rdtsc();
            micros = (stopTime.tv_usec - startTime.tv_usec) + (stopTime.tv_sec - startTime.tv_sec) * 1000000;
            if (micros > 10000) {
                cycles_persec = static_cast<double>(stopCycles - startCycles);
                cycles_persec = 1000000.0 * cycles_persec / static_cast<double>(micros);
                break;
            }
        }
        double delta = cycles_persec / 100000.0;
        if ((oldCycles > (cycles_persec - delta)) && (oldCycles < (cycles_persec + delta))) {
            goto exit;
        }
        oldCycles = cycles_persec;
    }
exit:;
}

double cycles::to_seconds(int64_t cycles, double cycles_persec)
{
    if (cycles_persec == 0)
        cycles_persec = get_cycles_persec();
    return static_cast<double>(cycles) / cycles_persec;
}

int get_current_timezone()
{
    return lc_time_zone;
}

} // namespace Argus
