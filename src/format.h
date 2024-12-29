#pragma once
#include "common.h"
#include <cstddef>
/*
    printf style check support reference from
    https://cplusplus.com/reference/cstdio/printf/
    A format specifier follows this prototype: [see compatibility note below]
    %[flags][width][.precision][length]specifier
*/

namespace Argus
{

constexpr inline bool is_flag(const char c)
{
    return c == '-' || c == '+' || c == ' ' || c == '#' || c == '0';
}

constexpr inline bool is_specifier(char c)
{
    return

        c == 'd' || c == 'i'
        || c == 'u' || c == 'o'
        || c == 'x' || c == 'X'
        || c == 'f' || c == 'F'
        || c == 'e' || c == 'E'
        || c == 'g' || c == 'G'
        || c == 'a' || c == 'A'
        || c == 'c' || c == 'p'
        || c == '%' || c == 's'
        || c == 'n';
}

constexpr inline bool is_length(char c)
{
    return c == 'h' || c == 'l' || c == 'j'
           || c == 'z' || c == 't' || c == 'L';
}

constexpr inline bool is_digit(char c)
{
    return (c >= '0' && c <= '9');
}

/*********************************************************************
 * ref %[flags][width][.precision][length]specifier
 * support types are whin the switch list
 */
template <int N>
constexpr inline PARAM_TYPE extract_param(const char (&fmt)[N],
                                          int idx = 0)
{
    int pos = 0;
    while (pos < N - 1) {
        if (fmt[pos] != '%') {
            ++pos;
            continue;
        } else {
            ++pos;
            while (!is_specifier(fmt[pos])) {
                ++pos;
            }
            // now we have the specifiler
            if (idx != 0) {
                --idx;
                ++pos;
                continue;

            } else {
                switch (fmt[pos]) {
                case 'd':
                case 'u':
                case 'f':
                case 'p':
                default:
                    return PARAM_TYPE::FIXED;
                case 's': return PARAM_TYPE::STRING;
                }
            }
        }
    }
    return PARAM_TYPE::INVALID;
}

template <int N>
constexpr inline int count_fmt_params(const char (&fmt)[N])
{
    int count = 0;
    while (extract_param(fmt, count) != PARAM_TYPE::INVALID)
        ++count;
    return count;
}

template <int N>
constexpr inline int count_string_params(const char (&fmt)[N])
{
    int count = 0;
    while (extract_param(fmt, count) == PARAM_TYPE::STRING)
        ++count;
    return count;
}

template <int N, std::size_t... Indices>
constexpr std::array<PARAM_TYPE, sizeof...(Indices)>
fmtstr2cst_array_internal(const char (&fmt)[N], std::index_sequence<Indices...>)
{
    return {{extract_param(fmt, Indices)...}};
}

template <int NParams, size_t N>
constexpr std::array<PARAM_TYPE, NParams>
fmtstr2cst_array(const char (&fmt)[N])
{
    return fmtstr2cst_array_internal(fmt, std::make_index_sequence<NParams>{});
}

enum
{
    StaticStringLiteral,
    StaticStringConcat
};

template <size_t N, int type>
struct static_string;

template <size_t N>
struct static_string<N, StaticStringLiteral>
{
    constexpr static_string(const char (&s)[N + 1]) :
        s(s) {}
    const char (&s)[N + 1];
};

template <size_t N>
struct static_string<N, StaticStringConcat>
{
    template <size_t N1, int T1, size_t N2, int T2>
    constexpr static_string(const static_string<N1, T1> &s1,
                            const static_string<N2, T2> &s2) :
        static_string(s1,
                      std::make_index_sequence<N1>{},
                      s2,
                      std::make_index_sequence<N2>{})
    {}

    template <size_t N1, int T1, size_t... I1, size_t N2, int T2, size_t... I2>
    constexpr static_string(const static_string<N1, T1> &s1,
                            std::index_sequence<I1...>,
                            const static_string<N2, T2> &s2,
                            std::index_sequence<I2...>) :
        s{s1.s[I1]..., s2.s[I2]..., '\0'}
    {
        static_assert(N == N1 + N2, "static_string length error");
    }

    char s[N + 1];
};

template <size_t N>
constexpr auto string_literal(const char (&s)[N])
{
    return static_string<N - 1, StaticStringLiteral>(s);
}

template <size_t N1, int T1, size_t N2, int T2>
constexpr auto string_concat(const static_string<N1, T1> &s1,
                             const static_string<N2, T2> &s2)
{
    return static_string<N1 + N2, StaticStringConcat>(s1, s2);
}

} // namespace Argus
