#pragma once
#include <cstdint>
#ifdef _WIN32
#include <intrin.h>
#include <windows.h>
#else
#include <sys/time.h>
#endif

namespace Argus
{

struct time_spot
{
    // rdtsc() time that corresponds with the unixTime below
    uint64_t rdtsc;

    // std::time() that corresponds with the rdtsc() above
    time_t unixtime;

    // Conversion factor between cycles returned by rdtsc() and 1 second
    double cycles_persecond;
};
/**
 * This class provides static methods that read the fine-grain CPU
 * cycle counter and translate between cycle-level times and absolute
 * times.
 */
class cycles
{
public:
    static void init();
    /**
     * Return the current value of the fine-grain CPU cycle counter
     * (accessed via the RDTSC instruction).
     */

#ifdef _WIN32
    inline static uint64_t rdtsc()
    {
        return __rdtsc();
    }
#else
#if defined(__aarch64__)
    inline static uint64_t rdtsc()
    {
        uint64_t tsc;
        asm volatile("mrs %0, cntvct_el0"
                     : "=r"(tsc));
        return tsc;
    }
#elif defined(__x86_64__) || defined(__i386__)
    static __inline __attribute__((always_inline))
    uint64_t
    rdtsc()
    {
        uint32_t lo, hi;
        __asm__ __volatile__("rdtsc"
                             : "=a"(lo), "=d"(hi));
        return (((uint64_t)hi << 32) | lo);
    }
#endif
#endif
    static double to_seconds(int64_t cycles, double cycles_persec = 0);

private:
    cycles();
    /// Conversion factor between cycles and the seconds; computed by
    /// Cycles::init.
    static double cycles_persec;

public:
    /**
     * Returns the conversion factor between cycles in seconds, using
     * a mock value for testing when appropriate.
     */
    inline static double
    get_cycles_persec()
    {
        return cycles_persec;
    }
};

int get_current_timezone();

} // namespace Argus
