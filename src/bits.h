//
//  bits.h
//

#ifndef bits_h
#define bits_h

/* type half maximum signed value */
template <typename R>
static inline R type_half_max_signed() { return R(1) << (sizeof(R) * 8 - 2); }

/* type maximum signed value */
template <typename R>
static inline R type_max_signed() { return type_half_max_signed<R>() - R(1) + type_half_max_signed<R>(); }

/* type minimum signed value */
template <typename R>
static inline R type_min_signed() { return R(-1) - type_max_signed<R>(); }

/* type minimum value */
template <typename R>
static inline R type_min() { return R(-1) < R(1) ? type_min_signed<R>() : R(0); }

/* type maximum value */
template <typename R>
static inline R type_max() { return R(~type_min<R>()); }

/* check add overflow */
template <typename R>
static bool add_overflow(R a, R b)
{
    return b < 1 ? (type_min<R>() - b <= a ? false : true) : (type_max<R>() - b >= a ? false : true);
}

/* check subtract underflow */
template <typename R>
static bool sub_underflow(R a, R b)
{
    return b < 1 ? (type_max<R>() + b >= a ? false : true) : (type_min<R>() + b <= a ? false : true);
}

/* convert to binary string */
template <typename T>
static inline std::string to_binary_string(T val)
{
    const int bits = sizeof(T) << 3;
    char buf[bits+1];
    for (int i = 0; i < bits; ++i) {
        buf[bits-i-1] = val & T(1)<<i ? '1' : '0';
    }
    buf[bits] = '\0';
    return buf;
}

/* count number of one bits in constant time */
template <typename T>
static inline uint32_t popcount(T val)
{
    const int bits = sizeof(T) << 3;
    uint32_t count = 0;
    for (int i = 0; i < bits; ++i) {
        count += val & T(1)<<i ? 1 : 0;
    }
    return count;
}


/* count leading zero bits in constant time */
template <typename T>
static inline uint32_t clz(T val)
{
    const int bits = sizeof(T) << 3;
    uint32_t count = 0, found = 0;
    for (int i = bits - 1; i >= 0; --i) {
        count += !(found |= val & T(1)<<i ? 1 : 0);
    }
    return count;
}

/* count trailing zero bits in constant time */
template <typename T>
static inline uint32_t ctz(T val)
{
    const int bits = sizeof(T) << 3;
    uint32_t count = 0, found = 0;
    for (int i = 0; i < bits; ++i) {
        count += !(found |= val & T(1)<<i ? 1 : 0);
    }
    return count;
}

/* return whether number is a power of two in constant time */
template <typename T>
static inline bool ispow2(T val)
{
    return val && !(val & (val-1));
}

/* return number rounded to the nearest power of two */
template <typename T>
static inline T roundpow2(T val)
{
    return ispow2(val) ? val :  1 << ((sizeof(val) << 3) - clz(val));
}

/* function template specializations */

/* count number of one bits - GCC uint16_t, uint32_t and uint64_t */
#if defined __GNUC__
#define __POPCOUNT_16_DEFINED
#define __POPCOUNT_32_DEFINED
#define __POPCOUNT_64_DEFINED
template <>
inline uint32_t __attribute__((__always_inline__)) popcount<uint16_t>(uint16_t val)
{
    return __builtin_popcount(uint32_t(val));
}

template <>
inline uint32_t __attribute__((__always_inline__)) popcount<uint32_t>(uint32_t val)
{
    return __builtin_popcount(val);
}

template <>
inline uint32_t __attribute__((__always_inline__)) popcount<uint64_t>(uint64_t val)
{
    return __builtin_popcountll(val);
}
#endif

/* count number of one bits - MSC uint16_t and uint32_t */
#if defined _MSC_VER && (defined _M_X64 || defined _M_IX86)
#define __POPCOUNT_16_DEFINED
#define __POPCOUNT_32_DEFINED
template <>
inline uint32_t popcount<uint16_t>(uint16_t val)
{
    return __popcnt(uint32_t(val));
}

template <>
inline uint32_t popcount<uint32_t>(uint32_t val)
{
    return __popcnt(val);
}
#endif

/* count number of one bits - MSC uint64_t */
#if defined _MSC_VER && defined _M_X64
#define __POPCOUNT_64_DEFINED
template <>
inline uint32_t popcount<uint64_t>(uint64_t val)
{
    return __popcnt64(val);
}
#elif defined _MSC_VER && defined _M_IX86
#define __POPCOUNT_64_DEFINED
template <>
inline uint32_t popcount<uint64_t>(uint64_t val)
{
    return __popcnt(uint32_t(val >> 32)) + __popcnt(uint32_t(val));
}
#endif


/* popcount implementation credits:  http://www.hackersdelight.org/hdcodetxt/pop.c.txt */

#ifndef __POPCOUNT_16_DEFINED
template <>
inline uint32_t popcount<uint16_t>(uint16_t val)
{
    val = (val & 0x5555) + ((val >> 1) & 0x5555);
    val = (val & 0x3333) + ((val >> 2) & 0x3333);
    val = (val & 0x0F0F) + ((val >> 4) & 0x0F0F);
    val = (val & 0x00FF) + ((val >> 8) & 0x00FF);
    return val;
}
#endif

#ifndef __POPCOUNT_32_DEFINED
template <>
inline uint32_t popcount<uint32_t>(uint32_t val)
{
    val = (val & 0x55555555) + ((val >> 1) & 0x55555555);
    val = (val & 0x33333333) + ((val >> 2) & 0x33333333);
    val = (val & 0x0F0F0F0F) + ((val >> 4) & 0x0F0F0F0F);
    val = (val & 0x00FF00FF) + ((val >> 8) & 0x00FF00FF);
    val = (val & 0x0000FFFF) + ((val >>16) & 0x0000FFFF);
    return val;
}
#endif

#ifndef __POPCOUNT_64_DEFINED
template <>
inline uint32_t popcount<uint64_t>(uint64_t val)
{
    val = (val & 0x5555555555555555ULL) + ((val >>  1) & 0x5555555555555555ULL);
    val = (val & 0x3333333333333333ULL) + ((val >>  2) & 0x3333333333333333ULL);
    val = (val & 0x0F0F0F0F0F0F0F0FULL) + ((val >>  4) & 0x0F0F0F0F0F0F0F0FULL);
    val = (val & 0x00FF00FF00FF00FFULL) + ((val >>  8) & 0x00FF00FF00FF00FFULL);
    val = (val & 0x0000FFFF0000FFFFULL) + ((val >> 16) & 0x0000FFFF0000FFFFULL);
    return uint32_t(val)                + uint32_t(val >> 32);
}
#endif

/* count leading zero bits - GCC uint16_t, uint32_t and uint64_t */
#if defined __GNUC__
template <>
inline uint32_t __attribute__((__always_inline__)) clz<uint16_t>(uint16_t val)
{
    int count = __builtin_clz(uint32_t(val));
    return (val == 0) ? 16 : count - 16;
}

template <>
inline uint32_t __attribute__((__always_inline__)) clz<uint32_t>(uint32_t val)
{
    int count = __builtin_clz(val);
    return (val == 0) ? 32 : count;
}

template <>
inline uint32_t __attribute__((__always_inline__)) clz<uint64_t>(uint64_t val)
{
    int count =__builtin_clzll(val);
    return (val == 0) ? 64 : count;
}
#endif


/* count leading zero bits - MSC uint16_t, uint32_t */
#if defined _MSC_VER && (defined _M_X64 || defined _M_IX86)
template <>
inline uint32_t clz<uint16_t>(uint16_t val)
{
    unsigned long count;
    return _BitScanReverse(&count, uint32_t(val)) ? 15 - count : 16;
}

template <>
inline uint32_t clz<uint32_t>(uint32_t val)
{
    unsigned long count;
    return _BitScanReverse(&count, val) ? 31 - count : 32;
}
#endif

/* count leading zero bits - MSC && X64 uint32_t */
#if defined _MSC_VER && defined _M_X64
template <>
inline uint32_t clz<uint64_t>(uint64_t val)
{
    unsigned long count;
    return _BitScanReverse64(&count, val) ? 63 - count : 64;
}
#elif defined _MSC_VER && defined _M_IX86
template <>
inline uint32_t clz<uint64_t>(uint64_t val)
{
    unsigned long hi_count;
    unsigned long lo_count;
    int hi_res = _BitScanReverse(&hi_count, uint32_t(val >> 32));
    int lo_res = _BitScanReverse(&lo_count, uint32_t(val));
    return hi_res ? 31 - hi_count : (lo_res ? 63 - lo_count : 64);
}
#endif

/* count trailing zero bits - GCC uint32_t and uint64_t */
#if defined __GNUC__
template <>
inline uint32_t __attribute__((__always_inline__)) ctz<uint16_t>(uint16_t val)
{
    int count =__builtin_ctz(uint32_t(val));
    return (val == 0) ? 16 : count;
}

template <>
inline uint32_t __attribute__((__always_inline__)) ctz<uint32_t>(uint32_t val)
{
    int count =__builtin_ctz(val);
    return (val == 0) ? 32 : count;
}

template <>
inline uint32_t __attribute__((__always_inline__)) ctz<uint64_t>(uint64_t val)
{
    int count =__builtin_ctzll(val);
    return (val == 0) ? 64 : count;
}
#endif

/* count trailing zero bits - MSC uint32_t */
#if defined _MSC_VER && (defined _M_X64 || defined _M_IX86)
template <>
inline uint32_t ctz<uint16_t>(uint16_t val)
{
    unsigned long count;
    return _BitScanForward(&count, uint32_t(val)) ? count : 16;
}

template <>
inline uint32_t ctz<uint32_t>(uint32_t val)
{
    unsigned long count;
    return _BitScanForward(&count, val) ? count : 32;
}
#endif

/* count trailing zero bits - MSC && X64 uint64_t */
#if defined _MSC_VER && defined _M_X64
template <>
inline uint32_t ctz<uint64_t>(uint64_t val)
{
    unsigned long count;
    return _BitScanForward64(&count, val) ? count : 64;
}
#elif defined _MSC_VER && defined _M_IX86
template <>
inline uint32_t ctz<uint64_t>(uint64_t val)
{
    unsigned long hi_count;
    unsigned long lo_count;
    int hi_res = _BitScanForward(&hi_count, uint32_t(val >> 32));
    int lo_res = _BitScanForward(&lo_count, uint32_t(val));
    return lo_res ? lo_count : (hi_res ? hi_count + 32 : 64);
}
#endif

#endif
