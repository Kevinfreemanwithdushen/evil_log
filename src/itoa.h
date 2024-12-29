#include <string>
#include <numeric>
#include <functional>
#include <cassert>

namespace Argus
{

namespace core
{

template <typename Char>
std::basic_string_view<Char> itoa2_core(std::uintmax_t num)
{
    static_assert(std::numeric_limits<unsigned char>::digits % 4 == 0);
    // 二进制4位表
    static const Char s_digits_table[]{
        // 0000 0001 0010 0011 -> 0 1 2 3
        '0', '0', '0', '0', '0', '0', '0', '1', '0', '0', '1', '0', '0', '0', '1', '1',
        // 0100 0101 0110 0111 -> 4 5 6 7
        '0', '1', '0', '0', '0', '1', '0', '1', '0', '1', '1', '0', '0', '1', '1', '1',
        // 1000 1001 1010 1011 -> 8 9 10 11
        '1', '0', '0', '0', '1', '0', '0', '1', '1', '0', '1', '0', '1', '0', '1', '1',
        // 1100 1101 1110 1111 -> 12 13 14 15
        '1', '1', '0', '0', '1', '1', '0', '1', '1', '1', '1', '0', '1', '1', '1', '1'};

    // 1 for '\0'
    constexpr auto ts_buf_len = std::numeric_limits<decltype(num)>::digits + 1;
    static thread_local Char ts_buf[ts_buf_len]{0};
    auto *it = &ts_buf[ts_buf_len - 1];

    do {
        it -= 4;
        memcpy(it, &s_digits_table[(num - ((num >> 4) << 4)) * 4], 4 * sizeof(Char));
        num >>= 4;
    } while (num != 0);

    assert(ts_buf[ts_buf_len - 1] == 0);
    assert(it >= ts_buf);

    while (*it == '0' && it < &ts_buf[ts_buf_len - 2])
        it++;

    return std::basic_string_view<Char>(it, &ts_buf[ts_buf_len - 1] - it);
}

template <typename Char>
std::basic_string_view<Char> itoa8_core(std::uintmax_t num)
{
    // 1 for ceiling, 1 for '\0'
    constexpr auto ts_buf_len = std::numeric_limits<decltype(num)>::digits / 3 + 2;
    static thread_local Char ts_buf[ts_buf_len]{0}; // thread safe
    // iterator
    auto *it = &ts_buf[ts_buf_len - 1];

    // begin
    do {
        *(--it) = '0' + (num - ((num >> 3) << 3));
        num >>= 3;
    } while (num != 0);

    assert(ts_buf[ts_buf_len - 1] == 0);
    assert(it >= ts_buf);

    return std::string_view(it, &ts_buf[ts_buf_len - 1] - it);
}

template <typename Char>
std::basic_string_view<Char> itoa10_core(std::uintmax_t num)
{
    // split up to fit in 'Char', 十进制双数位表
    static const Char s_digits_table[]{
        // 00 01 02 03 04 05 06 07 08 09
        '0', '0', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', '0', '6', '0', '7', '0', '8', '0', '9',
        // 10 11 12 13 14 15 16 17 18 19
        '1', '0', '1', '1', '1', '2', '1', '3', '1', '4', '1', '5', '1', '6', '1', '7', '1', '8', '1', '9',
        // 20 21 22 23 24 25 26 27 28 29
        '2', '0', '2', '1', '2', '2', '2', '3', '2', '4', '2', '5', '2', '6', '2', '7', '2', '8', '2', '9',
        // 30 31 32 33 34 35 36 37 38 39
        '3', '0', '3', '1', '3', '2', '3', '3', '3', '4', '3', '5', '3', '6', '3', '7', '3', '8', '3', '9',
        // 40 41 42 43 44 45 46 47 48 49
        '4', '0', '4', '1', '4', '2', '4', '3', '4', '4', '4', '5', '4', '6', '4', '7', '4', '8', '4', '9',
        // 50 51 52 53 54 55 56 57 58 59
        '5', '0', '5', '1', '5', '2', '5', '3', '5', '4', '5', '5', '5', '6', '5', '7', '5', '8', '5', '9',
        // 60 61 62 63 64 65 66 67 68 69
        '6', '0', '6', '1', '6', '2', '6', '3', '6', '4', '6', '5', '6', '6', '6', '7', '6', '8', '6', '9',
        // 70 71 72 73 74 75 76 77 78 79
        '7', '0', '7', '1', '7', '2', '7', '3', '7', '4', '7', '5', '7', '6', '7', '7', '7', '8', '7', '9',
        // 80 81 82 83 84 85 86 87 88 89
        '8', '0', '8', '1', '8', '2', '8', '3', '8', '4', '8', '5', '8', '6', '8', '7', '8', '8', '8', '9',
        // 90 91 92 93 94 95 96 97 98 99
        '9', '0', '9', '1', '9', '2', '9', '3', '9', '4', '9', '5', '9', '6', '9', '7', '9', '8', '9', '9'};

    // digits10 is the value of digits multiplied by log10(2) and rounded down.
    // 1 for ceiling, lcm for copying pair, 1 for '\0'
    constexpr auto ts_buf_len = std::lcm(std::numeric_limits<decltype(num)>::digits10 + 1, 2) + 1;
    // thread safe
    static thread_local Char ts_buf[ts_buf_len]{0};
    // iterator
    auto *it = &ts_buf[ts_buf_len - 1];

    // begin
    auto div = num / 100;
    while (div != 0) {
        it -= 2;
        // "num - div * 100" is equal to "num % 100"
        memcpy(it, &s_digits_table[2 * (num - div * 100)], 2 * sizeof(Char));
        num = div;
        div /= 100;
    }
    // still remain 1 or 2 digits
    it -= 2;
    memcpy(it, &s_digits_table[2 * num], 2 * sizeof(Char));

    assert(ts_buf[ts_buf_len - 1] == 0);
    assert(it >= ts_buf);

    if (num < 10) // skip '0'
        it++;

    // [??, ??)
    return std::basic_string_view<Char>(it, &ts_buf[ts_buf_len - 1] - it);
}

template <typename Char>
std::basic_string_view<Char> itoa16_core(std::uintmax_t num)
{
    static Char s_digits_table[]{
        '0', '0', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', '0', '6', '0', '7', '0', '8', '0', '9', '0', 'a',
        '0', 'b', '0', 'c', '0', 'd', '0', 'e', '0', 'f',
        '1', '0', '1', '1', '1', '2', '1', '3', '1', '4', '1', '5', '1', '6', '1', '7', '1', '8', '1', '9', '1', 'a',
        '1', 'b', '1', 'c', '1', 'd', '1', 'e', '1', 'f',
        '2', '0', '2', '1', '2', '2', '2', '3', '2', '4', '2', '5', '2', '6', '2', '7', '2', '8', '2', '9', '2', 'a',
        '2', 'b', '2', 'c', '2', 'd', '2', 'e', '2', 'f',
        '3', '0', '3', '1', '3', '2', '3', '3', '3', '4', '3', '5', '3', '6', '3', '7', '3', '8', '3', '9', '3', 'a',
        '3', 'b', '3', 'c', '3', 'd', '3', 'e', '3', 'f',
        '4', '0', '4', '1', '4', '2', '4', '3', '4', '4', '4', '5', '4', '6', '4', '7', '4', '8', '4', '9', '4', 'a',
        '4', 'b', '4', 'c', '4', 'd', '4', 'e', '4', 'f',
        '5', '0', '5', '1', '5', '2', '5', '3', '5', '4', '5', '5', '5', '6', '5', '7', '5', '8', '5', '9', '5', 'a',
        '5', 'b', '5', 'c', '5', 'd', '5', 'e', '5', 'f',
        '6', '0', '6', '1', '6', '2', '6', '3', '6', '4', '6', '5', '6', '6', '6', '7', '6', '8', '6', '9', '6', 'a',
        '6', 'b', '6', 'c', '6', 'd', '6', 'e', '6', 'f',
        '7', '0', '7', '1', '7', '2', '7', '3', '7', '4', '7', '5', '7', '6', '7', '7', '7', '8', '7', '9', '7', 'a',
        '7', 'b', '7', 'c', '7', 'd', '7', 'e', '7', 'f',
        '8', '0', '8', '1', '8', '2', '8', '3', '8', '4', '8', '5', '8', '6', '8', '7', '8', '8', '8', '9', '8', 'a',
        '8', 'b', '8', 'c', '8', 'd', '8', 'e', '8', 'f',
        '9', '0', '9', '1', '9', '2', '9', '3', '9', '4', '9', '5', '9', '6', '9', '7', '9', '8', '9', '9', '9', 'a',
        '9', 'b', '9', 'c', '9', 'd', '9', 'e', '9', 'f',
        'a', '0', 'a', '1', 'a', '2', 'a', '3', 'a', '4', 'a', '5', 'a', '6', 'a', '7', 'a', '8', 'a', '9', 'a', 'a',
        'a', 'b', 'a', 'c', 'a', 'd', 'a', 'e', 'a', 'f',
        'b', '0', 'b', '1', 'b', '2', 'b', '3', 'b', '4', 'b', '5', 'b', '6', 'b', '7', 'b', '8', 'b', '9', 'b', 'a',
        'b', 'b', 'b', 'c', 'b', 'd', 'b', 'e', 'b', 'f',
        'c', '0', 'c', '1', 'c', '2', 'c', '3', 'c', '4', 'c', '5', 'c', '6', 'c', '7', 'c', '8', 'c', '9', 'c', 'a',
        'c', 'b', 'c', 'c', 'c', 'd', 'c', 'e', 'c', 'f',
        'd', '0', 'd', '1', 'd', '2', 'd', '3', 'd', '4', 'd', '5', 'd', '6', 'd', '7', 'd', '8', 'd', '9', 'd', 'a',
        'd', 'b', 'd', 'c', 'd', 'd', 'd', 'e', 'd', 'f',
        'e', '0', 'e', '1', 'e', '2', 'e', '3', 'e', '4', 'e', '5', 'e', '6', 'e', '7', 'e', '8', 'e', '9', 'e', 'a',
        'e', 'b', 'e', 'c', 'e', 'd', 'e', 'e', 'e', 'f',
        'f', '0', 'f', '1', 'f', '2', 'f', '3', 'f', '4', 'f', '5', 'f', '6', 'f', '7', 'f', '8', 'f', '9', 'f', 'a',
        'f', 'b', 'f', 'c', 'f', 'd', 'f', 'e', 'f', 'f'};

    constexpr auto ts_buf_len = std::numeric_limits<decltype(num)>::digits / 4 + 1;
    static thread_local Char ts_buf[ts_buf_len]{0};
    auto *it = &ts_buf[ts_buf_len - 1];

    do {
        it -= 2;
        memcpy(it, &s_digits_table[(num - ((num >> 8) << 8)) * 2], 2 * sizeof(Char));
        num >>= 8;
    } while (num != 0);

    assert(ts_buf[ts_buf_len - 1] == 0);
    assert(it >= ts_buf);

    if (*it == '0')
        it++;

    return std::basic_string_view<Char>(it, &ts_buf[ts_buf_len - 1] - it);
}

template <typename Char>
std::basic_string_view<Char> handle_signed(std::function<std::basic_string_view<Char>(std::uintmax_t)> F,
                                           std::intmax_t num)
{
    bool is_negative = num < 0;
    if (is_negative) {
        num = -num;
        auto result = F(static_cast<std::uintmax_t>(num));
        *const_cast<Char *>(result.data() - 1) = '-';
        return std::basic_string_view<Char>(result.data() - 1, result.length() + 1);
    } else {
        return F(static_cast<std::uintmax_t>(num));
    }
}

} // namespace core

template <typename Char, typename T>
std::basic_string_view<Char> itoa2(T num)
{
    static_assert(std::is_integral_v<T>);
    if constexpr (std::is_signed_v<T>) {
        return core::handle_signed<Char>(core::itoa2_core<Char>, num);
    } else {
        return core::itoa2_core<Char>(num);
    }
}

template <typename Char, typename T>
std::basic_string_view<Char> itoa8(T num)
{
    static_assert(std::is_integral_v<T>);
    if constexpr (std::is_signed_v<T>) {
        return core::handle_signed<Char>(core::itoa8_core<Char>, num);
    } else {
        return core::itoa8_core<Char>(num);
    }
}

template <typename Char, typename T>
std::basic_string_view<Char> itoa10(T num)
{
    static_assert(std::is_integral_v<T>);
    if constexpr (std::is_signed_v<T>) {
        return core::handle_signed<Char>(core::itoa10_core<Char>, num);
    } else {
        return core::itoa10_core<Char>(static_cast<std::uintmax_t>(num));
    }
}

template <typename Char, typename T>
std::basic_string_view<Char> itoa16(T num)
{
    static_assert(std::is_integral_v<T>);
    if constexpr (std::is_signed_v<T>) {
        return core::handle_signed<Char>(core::itoa16_core<Char>, num);
    } else {
        return core::itoa16_core<Char>(num);
    }
}

} // namespace Argus