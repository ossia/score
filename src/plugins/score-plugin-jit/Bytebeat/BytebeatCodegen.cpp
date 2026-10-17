#include "BytebeatCodegen.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace Jit::Bytebeat
{
namespace
{
// Shared by both languages: number classification without relying on the
// floating-point environment (the JIT may enable -ffinite-math-only), and the
// conversion of a formula value to a sample.
constexpr std::string_view runtime_common = R"_(
#pragma float_control(precise, on)
namespace bb_rt
{
using u64 = unsigned long long;
using u32 = unsigned int;
using i32 = int;

inline constexpr bool is_nan(double x) noexcept
{
  return (__builtin_bit_cast(u64, x) & 0x7fffffffffffffffull) > 0x7ff0000000000000ull;
}
inline constexpr double nan_v = __builtin_bit_cast(double, 0x7ff8000000000000ull);
inline constexpr double inf_v = __builtin_bit_cast(double, 0x7ff0000000000000ull);

// ECMAScript ToInt32: truncate, then wrap modulo 2^32. NaN and infinities give 0.
inline constexpr i32 to_int32(double x) noexcept
{
  const u64 bits = __builtin_bit_cast(u64, x);
  const int exponent = int((bits >> 52) & 0x7ff);
  if(exponent == 0x7ff || exponent < 1023)
    return 0;
  if(exponent <= 1023 + 30)
    return i32(x);
  const int shift = exponent - 1023 - 52;
  if(shift >= 32)
    return 0;
  const u64 mantissa = (bits & 0xfffffffffffffull) | 0x10000000000000ull;
  u32 r = shift >= 0 ? u32(mantissa << shift) : u32(mantissa >> -shift);
  if(bits >> 63)
    r = 0u - r;
  return i32(r);
}

// The 8-bit sample values, b / 127.5 - 1: a load instead of a division, and
// unlike a multiplication by 1 / 127.5 it cannot be fused differently per CPU.
struct byte_samples
{
  double v[256];
  constexpr byte_samples() noexcept
      : v{}
  {
    for(int b = 0; b < 256; b++)
      v[b] = b / 127.5 - 1.;
  }
};
inline constexpr byte_samples byte_sample{};

// Run once per formula and per sample: always inlined
[[gnu::always_inline]] inline bool to_sample(double x, int mode, double& out) noexcept
{
  if(is_nan(x))
    return false;
  switch(mode)
  {
    default:
    case 0:
      out = byte_sample.v[u32(to_int32(x)) & 255u];
      break;
    case 1:
      out = byte_sample.v[u32(to_int32(x + 128.)) & 255u];
      break;
    case 2:
      out = x < -1. ? -1. : x > 1. ? 1. : x;
      break;
  }
  return true;
}

// A value that is not a number keeps the previous sample, as in the dollchan player.
[[gnu::always_inline]] inline void mix(double* hold, int mode, double l, double r, double& L, double& R) noexcept
{
  double s;
  if(to_sample(l, mode, s))
    hold[0] = s;
  if(to_sample(r, mode, s))
    hold[1] = s;
  L += hold[0];
  R += hold[1];
}

struct state_base
{
  double last_t;
  int has_last;
  double out[2];
  u64 rng;

  double random() noexcept
  {
    if(!rng)
      rng = 0x9E3779B97F4A7C15ull;
    rng ^= rng >> 12;
    rng ^= rng << 25;
    rng ^= rng >> 27;
    return double((rng * 2685821657736338717ull) >> 11) * (1. / 9007199254740992.);
  }
};
}
)_";

// ECMAScript number semantics.
constexpr std::string_view runtime_js = R"_(
namespace js
{
using namespace bb_rt;

struct num
{
  double v{};
  constexpr num() noexcept = default;
  constexpr num(double x) noexcept
      : v{x}
  {
  }
  explicit constexpr operator bool() const noexcept { return v != 0. && !is_nan(v); }

  constexpr num operator+() const noexcept { return *this; }
  constexpr num operator-() const noexcept { return -v; }
  constexpr num operator~() const noexcept { return double(~to_int32(v)); }

  num& operator++() noexcept { v += 1.; return *this; }
  num& operator--() noexcept { v -= 1.; return *this; }
  num operator++(int) noexcept { num r = *this; v += 1.; return r; }
  num operator--(int) noexcept { num r = *this; v -= 1.; return r; }

  num& operator+=(num o) noexcept;
  num& operator-=(num o) noexcept;
  num& operator*=(num o) noexcept;
  num& operator/=(num o) noexcept;
  num& operator%=(num o) noexcept;
  num& operator&=(num o) noexcept;
  num& operator|=(num o) noexcept;
  num& operator^=(num o) noexcept;
  num& operator<<=(num o) noexcept;
  num& operator>>=(num o) noexcept;
};

inline constexpr u32 shift_count(num b) noexcept { return u32(to_int32(b.v)) & 31u; }

inline constexpr num operator+(num a, num b) noexcept { return a.v + b.v; }
inline constexpr num operator-(num a, num b) noexcept { return a.v - b.v; }
inline constexpr num operator*(num a, num b) noexcept { return a.v * b.v; }
inline constexpr num operator/(num a, num b) noexcept { return a.v / b.v; }
inline num operator%(num a, num b) noexcept { return __builtin_fmod(a.v, b.v); }
inline constexpr num operator&(num a, num b) noexcept { return double(to_int32(a.v) & to_int32(b.v)); }
inline constexpr num operator|(num a, num b) noexcept { return double(to_int32(a.v) | to_int32(b.v)); }
inline constexpr num operator^(num a, num b) noexcept { return double(to_int32(a.v) ^ to_int32(b.v)); }
inline constexpr num operator<<(num a, num b) noexcept
{
  return double(i32(u32(to_int32(a.v)) << shift_count(b)));
}
inline constexpr num operator>>(num a, num b) noexcept
{
  return double(to_int32(a.v) >> shift_count(b));
}

// a >>> b is written a >> ushr_t{} >> b: same precedence, same associativity.
struct ushr_t { };
struct ushr_lhs { double v; };
inline constexpr ushr_lhs operator>>(num a, ushr_t) noexcept { return {a.v}; }
inline constexpr num operator>>(ushr_lhs a, num b) noexcept
{
  return double(u32(to_int32(a.v)) >> shift_count(b));
}

inline constexpr bool operator<(num a, num b) noexcept { return !is_nan(a.v) && !is_nan(b.v) && a.v < b.v; }
inline constexpr bool operator>(num a, num b) noexcept { return !is_nan(a.v) && !is_nan(b.v) && a.v > b.v; }
inline constexpr bool operator<=(num a, num b) noexcept { return !is_nan(a.v) && !is_nan(b.v) && a.v <= b.v; }
inline constexpr bool operator>=(num a, num b) noexcept { return !is_nan(a.v) && !is_nan(b.v) && a.v >= b.v; }
inline constexpr bool operator==(num a, num b) noexcept { return !is_nan(a.v) && !is_nan(b.v) && a.v == b.v; }
inline constexpr bool operator!=(num a, num b) noexcept { return !(a == b); }

inline num& num::operator+=(num o) noexcept { return *this = *this + o; }
inline num& num::operator-=(num o) noexcept { return *this = *this - o; }
inline num& num::operator*=(num o) noexcept { return *this = *this * o; }
inline num& num::operator/=(num o) noexcept { return *this = *this / o; }
inline num& num::operator%=(num o) noexcept { return *this = *this % o; }
inline num& num::operator&=(num o) noexcept { return *this = *this & o; }
inline num& num::operator|=(num o) noexcept { return *this = *this | o; }
inline num& num::operator^=(num o) noexcept { return *this = *this ^ o; }
inline num& num::operator<<=(num o) noexcept { return *this = *this << o; }
inline num& num::operator>>=(num o) noexcept { return *this = *this >> o; }

// Index of an array or a string: undefined unless it is an integer in range.
inline bool index_of(num i, int size, int& k) noexcept
{
  const double x = i.v;
  if(is_nan(x) || x < 0. || x >= double(size))
    return false;
  k = int(x);
  return double(k) == x;
}

template <typename T>
struct undefined_of
{
  static constexpr T get() noexcept { return T{}; }
};
template <>
struct undefined_of<num>
{
  static constexpr num get() noexcept { return nan_v; }
};

template <typename T, int N>
struct array
{
  T data[N > 0 ? N : 1];
  static constexpr num length{double(N)};

  constexpr T operator[](num i) const noexcept
  {
    int k;
    return index_of(i, N, k) ? data[k] : undefined_of<T>::get();
  }
};

// String literal, as UTF-16 code units like in JS.
struct str
{
  const char16_t* s{};
  int n{};
  num length{};

  str() noexcept = default;
  template <int N>
  constexpr str(const char16_t (&a)[N]) noexcept
      : s{a}
      , n{N - 1}
      , length{double(N - 1)}
  {
  }

  // s[i] is a one-character string; this is what it gives in arithmetic.
  constexpr num operator[](num i) const noexcept
  {
    int k;
    if(!index_of(i, n, k))
      return nan_v;
    const char16_t c = s[k];
    if(c >= u'0' && c <= u'9')
      return double(c - u'0');
    switch(c)
    {
      case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x20:
      case 0xa0: case 0x1680: case 0x2028: case 0x2029: case 0x202f:
      case 0x205f: case 0x3000: case 0xfeff:
        return 0.;
      default:
        return (c >= 0x2000 && c <= 0x200a) ? 0. : nan_v;
    }
  }

  constexpr num charCodeAt(num i = 0.) const noexcept
  {
    int k;
    return index_of(i, n, k) ? num(double(s[k])) : num(nan_v);
  }
};

template <typename T>
struct elem_of
{
  using type = num;
};
template <typename T, int N>
struct elem_of<array<T, N>>
{
  using type = array<T, N>;
};
template <>
struct elem_of<str>
{
  using type = str;
};

inline constexpr array<num, 0> arr() noexcept { return {}; }
template <typename A0, typename... A>
constexpr auto arr(A0 a0, A... a) noexcept
{
  using T = typename elem_of<A0>::type;
  return array<T, 1 + int(sizeof...(A))>{{T(a0), T(a)...}};
}

// Math, also available without the prefix as in the web players.
inline constexpr num E = 2.718281828459045;
inline constexpr num LN10 = 2.302585092994046;
inline constexpr num LN2 = 0.6931471805599453;
inline constexpr num LOG10E = 0.4342944819032518;
inline constexpr num LOG2E = 1.4426950408889634;
inline constexpr num PI = 3.141592653589793;
inline constexpr num SQRT1_2 = 0.7071067811865476;
inline constexpr num SQRT2 = 1.4142135623730951;

inline num abs(num x) noexcept { return __builtin_fabs(x.v); }
inline num acos(num x) noexcept { return __builtin_acos(x.v); }
inline num acosh(num x) noexcept { return __builtin_acosh(x.v); }
inline num asin(num x) noexcept { return __builtin_asin(x.v); }
inline num asinh(num x) noexcept { return __builtin_asinh(x.v); }
inline num atan(num x) noexcept { return __builtin_atan(x.v); }
inline num atanh(num x) noexcept { return __builtin_atanh(x.v); }
inline num atan2(num y, num x) noexcept { return __builtin_atan2(y.v, x.v); }
inline num cbrt(num x) noexcept { return __builtin_cbrt(x.v); }
inline num ceil(num x) noexcept { return __builtin_ceil(x.v); }
inline num cos(num x) noexcept { return __builtin_cos(x.v); }
inline num cosh(num x) noexcept { return __builtin_cosh(x.v); }
inline num exp(num x) noexcept { return __builtin_exp(x.v); }
inline num expm1(num x) noexcept { return __builtin_expm1(x.v); }
inline num floor(num x) noexcept { return __builtin_floor(x.v); }
inline num fround(num x) noexcept { return double(float(x.v)); }
inline num log(num x) noexcept { return __builtin_log(x.v); }
inline num log1p(num x) noexcept { return __builtin_log1p(x.v); }
inline num log10(num x) noexcept { return __builtin_log10(x.v); }
inline num log2(num x) noexcept { return __builtin_log2(x.v); }
inline num sin(num x) noexcept { return __builtin_sin(x.v); }
inline num sinh(num x) noexcept { return __builtin_sinh(x.v); }
inline num sqrt(num x) noexcept { return __builtin_sqrt(x.v); }
inline num tan(num x) noexcept { return __builtin_tan(x.v); }
inline num tanh(num x) noexcept { return __builtin_tanh(x.v); }
inline num trunc(num x) noexcept { return __builtin_trunc(x.v); }
inline num int_(num x) noexcept { return floor(x); }

inline num round(num x) noexcept
{
  if(is_nan(x.v))
    return x;
  const double f = __builtin_floor(x.v);
  return x.v - f >= 0.5 ? f + 1. : f;
}
inline constexpr num sign(num x) noexcept
{
  return is_nan(x.v) ? x : x.v > 0. ? num(1.) : x.v < 0. ? num(-1.) : x;
}
inline num pow(num a, num b) noexcept
{
  if(is_nan(b.v) || ((a.v == 1. || a.v == -1.) && (b.v == inf_v || b.v == -inf_v)))
    return nan_v;
  return __builtin_pow(a.v, b.v);
}
inline constexpr num clz32(num x) noexcept
{
  const u32 v = u32(to_int32(x.v));
  return v == 0 ? 32. : double(__builtin_clz(v));
}
inline constexpr num imul(num a, num b) noexcept
{
  return double(i32(u32(to_int32(a.v)) * u32(to_int32(b.v))));
}

inline constexpr num max() noexcept { return -inf_v; }
inline constexpr num min() noexcept { return inf_v; }
template <typename A0, typename... A>
constexpr num max(A0 a0, A... a) noexcept
{
  const num x = a0;
  if constexpr(sizeof...(A) == 0)
    return x;
  else
  {
    const num y = max(a...);
    return is_nan(x.v) || is_nan(y.v) ? num(nan_v) : x.v > y.v ? x : y;
  }
}
template <typename A0, typename... A>
constexpr num min(A0 a0, A... a) noexcept
{
  const num x = a0;
  if constexpr(sizeof...(A) == 0)
    return x;
  else
  {
    const num y = min(a...);
    return is_nan(x.v) || is_nan(y.v) ? num(nan_v) : x.v < y.v ? x : y;
  }
}
template <typename... A>
num hypot(A... a) noexcept
{
  double s = 0.;
  ((s += num(a).v * num(a).v), ...);
  return __builtin_sqrt(s);
}

[[gnu::always_inline]] inline void emit(double* hold, int mode, num v, double& l, double& r) noexcept
{
  mix(hold, mode, v.v, v.v, l, r);
}

// [left, right]
template <typename T, int N>
[[gnu::always_inline]] inline void emit(double* hold, int mode, const array<T, N>& v, double& l, double& r) noexcept
{
  const num a = v[num(0.)];
  const num b = v[num(1.)];
  mix(hold, mode, a.v, b.v, l, r);
}
}
)_";

// C semantics with 32-bit wrapping integers and no undefined behaviour.
constexpr std::string_view runtime_c = R"_(
namespace c
{
using namespace bb_rt;

template <typename T> struct is_int_ { static constexpr bool value = false; };
template <> struct is_int_<bool> { static constexpr bool value = true; };
template <> struct is_int_<char> { static constexpr bool value = true; };
template <> struct is_int_<signed char> { static constexpr bool value = true; };
template <> struct is_int_<unsigned char> { static constexpr bool value = true; };
template <> struct is_int_<char16_t> { static constexpr bool value = true; };
template <> struct is_int_<char32_t> { static constexpr bool value = true; };
template <> struct is_int_<wchar_t> { static constexpr bool value = true; };
template <> struct is_int_<short> { static constexpr bool value = true; };
template <> struct is_int_<unsigned short> { static constexpr bool value = true; };
template <> struct is_int_<int> { static constexpr bool value = true; };
template <> struct is_int_<unsigned int> { static constexpr bool value = true; };
template <> struct is_int_<long> { static constexpr bool value = true; };
template <> struct is_int_<unsigned long> { static constexpr bool value = true; };
template <> struct is_int_<long long> { static constexpr bool value = true; };
template <> struct is_int_<unsigned long long> { static constexpr bool value = true; };

template <typename T> struct is_float_ { static constexpr bool value = false; };
template <> struct is_float_<float> { static constexpr bool value = true; };
template <> struct is_float_<double> { static constexpr bool value = true; };
template <> struct is_float_<long double> { static constexpr bool value = true; };

struct c_int;
template <typename T> struct is_cint_ { static constexpr bool value = false; };
template <> struct is_cint_<c_int> { static constexpr bool value = true; };

template <typename T>
concept integral = is_int_<T>::value || is_cint_<T>::value;
template <typename A, typename B>
concept int_op = (is_cint_<A>::value || is_cint_<B>::value) && integral<A> && integral<B>;
template <typename A, typename B>
concept float_op = (is_cint_<A>::value && is_float_<B>::value)
                   || (is_float_<A>::value && is_cint_<B>::value);

struct c_int
{
  i32 v{};
  constexpr c_int() noexcept = default;
  constexpr explicit c_int(i32 x) noexcept
      : v{x}
  {
  }
  constexpr operator i32() const noexcept { return v; }

  constexpr c_int operator+() const noexcept { return *this; }
  constexpr c_int operator-() const noexcept { return c_int(i32(0u - u32(v))); }
  constexpr c_int operator~() const noexcept { return c_int(~v); }

  c_int& operator++() noexcept { v = i32(u32(v) + 1u); return *this; }
  c_int& operator--() noexcept { v = i32(u32(v) - 1u); return *this; }
  c_int operator++(int) noexcept { c_int r = *this; ++*this; return r; }
  c_int operator--(int) noexcept { c_int r = *this; --*this; return r; }

  template <integral B> c_int& operator+=(B b) noexcept;
  template <integral B> c_int& operator-=(B b) noexcept;
  template <integral B> c_int& operator*=(B b) noexcept;
  template <integral B> c_int& operator/=(B b) noexcept;
  template <integral B> c_int& operator%=(B b) noexcept;
  template <integral B> c_int& operator&=(B b) noexcept;
  template <integral B> c_int& operator|=(B b) noexcept;
  template <integral B> c_int& operator^=(B b) noexcept;
  template <integral B> c_int& operator<<=(B b) noexcept;
  template <integral B> c_int& operator>>=(B b) noexcept;
};

inline constexpr i32 as_i32(c_int x) noexcept { return x.v; }
template <typename T>
  requires is_int_<T>::value
constexpr i32 as_i32(T x) noexcept
{
  return i32(u32(x));
}
inline constexpr double as_f(c_int x) noexcept { return x.v; }
template <typename T>
  requires is_float_<T>::value
constexpr double as_f(T x) noexcept
{
  return double(x);
}

template <typename A, typename B> requires int_op<A, B>
constexpr c_int operator+(A a, B b) noexcept { return c_int(i32(u32(as_i32(a)) + u32(as_i32(b)))); }
template <typename A, typename B> requires int_op<A, B>
constexpr c_int operator-(A a, B b) noexcept { return c_int(i32(u32(as_i32(a)) - u32(as_i32(b)))); }
template <typename A, typename B> requires int_op<A, B>
constexpr c_int operator*(A a, B b) noexcept { return c_int(i32(u32(as_i32(a)) * u32(as_i32(b)))); }
template <typename A, typename B> requires int_op<A, B>
constexpr c_int operator/(A a, B b) noexcept
{
  const i32 x = as_i32(a), y = as_i32(b);
  if(y == 0)
    return c_int(0);
  if(y == -1)
    return c_int(i32(0u - u32(x)));
  return c_int(x / y);
}
template <typename A, typename B> requires int_op<A, B>
constexpr c_int operator%(A a, B b) noexcept
{
  const i32 x = as_i32(a), y = as_i32(b);
  if(y == 0 || y == -1)
    return c_int(0);
  return c_int(x % y);
}
template <typename A, typename B> requires int_op<A, B>
constexpr c_int operator&(A a, B b) noexcept { return c_int(as_i32(a) & as_i32(b)); }
template <typename A, typename B> requires int_op<A, B>
constexpr c_int operator|(A a, B b) noexcept { return c_int(as_i32(a) | as_i32(b)); }
template <typename A, typename B> requires int_op<A, B>
constexpr c_int operator^(A a, B b) noexcept { return c_int(as_i32(a) ^ as_i32(b)); }
template <typename A, typename B> requires int_op<A, B>
constexpr c_int operator<<(A a, B b) noexcept
{
  return c_int(i32(u32(as_i32(a)) << (u32(as_i32(b)) & 31u)));
}
template <typename A, typename B> requires int_op<A, B>
constexpr c_int operator>>(A a, B b) noexcept
{
  return c_int(as_i32(a) >> (u32(as_i32(b)) & 31u));
}

template <typename A, typename B> requires float_op<A, B>
constexpr double operator+(A a, B b) noexcept { return as_f(a) + as_f(b); }
template <typename A, typename B> requires float_op<A, B>
constexpr double operator-(A a, B b) noexcept { return as_f(a) - as_f(b); }
template <typename A, typename B> requires float_op<A, B>
constexpr double operator*(A a, B b) noexcept { return as_f(a) * as_f(b); }
template <typename A, typename B> requires float_op<A, B>
constexpr double operator/(A a, B b) noexcept { return as_f(a) / as_f(b); }

template <integral B> c_int& c_int::operator+=(B b) noexcept { return *this = *this + b; }
template <integral B> c_int& c_int::operator-=(B b) noexcept { return *this = *this - b; }
template <integral B> c_int& c_int::operator*=(B b) noexcept { return *this = *this * b; }
template <integral B> c_int& c_int::operator/=(B b) noexcept { return *this = *this / b; }
template <integral B> c_int& c_int::operator%=(B b) noexcept { return *this = *this % b; }
template <integral B> c_int& c_int::operator&=(B b) noexcept { return *this = *this & b; }
template <integral B> c_int& c_int::operator|=(B b) noexcept { return *this = *this | b; }
template <integral B> c_int& c_int::operator^=(B b) noexcept { return *this = *this ^ b; }
template <integral B> c_int& c_int::operator<<=(B b) noexcept { return *this = *this << b; }
template <integral B> c_int& c_int::operator>>=(B b) noexcept { return *this = *this >> b; }

// <math.h>
inline c_int abs(c_int x) noexcept { return x.v < 0 ? -x : x; }
inline int abs(int x) noexcept { return x < 0 ? int(0u - u32(x)) : x; }
inline double abs(double x) noexcept { return __builtin_fabs(x); }
inline double fabs(double x) noexcept { return __builtin_fabs(x); }
inline double acos(double x) noexcept { return __builtin_acos(x); }
inline double asin(double x) noexcept { return __builtin_asin(x); }
inline double atan(double x) noexcept { return __builtin_atan(x); }
inline double atan2(double y, double x) noexcept { return __builtin_atan2(y, x); }
inline double cbrt(double x) noexcept { return __builtin_cbrt(x); }
inline double ceil(double x) noexcept { return __builtin_ceil(x); }
inline double cos(double x) noexcept { return __builtin_cos(x); }
inline double cosh(double x) noexcept { return __builtin_cosh(x); }
inline double exp(double x) noexcept { return __builtin_exp(x); }
inline double floor(double x) noexcept { return __builtin_floor(x); }
inline double fmod(double x, double y) noexcept { return __builtin_fmod(x, y); }
inline double hypot(double x, double y) noexcept { return __builtin_hypot(x, y); }
inline double log(double x) noexcept { return __builtin_log(x); }
inline double log10(double x) noexcept { return __builtin_log10(x); }
inline double log2(double x) noexcept { return __builtin_log2(x); }
inline double pow(double x, double y) noexcept { return __builtin_pow(x, y); }
inline double round(double x) noexcept { return __builtin_round(x); }
inline double sin(double x) noexcept { return __builtin_sin(x); }
inline double sinh(double x) noexcept { return __builtin_sinh(x); }
inline double sqrt(double x) noexcept { return __builtin_sqrt(x); }
inline double tan(double x) noexcept { return __builtin_tan(x); }
inline double tanh(double x) noexcept { return __builtin_tanh(x); }
inline double trunc(double x) noexcept { return __builtin_trunc(x); }

// Kept for formulas written for the JS players
struct math_t
{
  static double abs(double x) noexcept { return __builtin_fabs(x); }
  static c_int abs(c_int x) noexcept { return c::abs(x); }
  static double acos(double x) noexcept { return __builtin_acos(x); }
  static double asin(double x) noexcept { return __builtin_asin(x); }
  static double atan(double x) noexcept { return __builtin_atan(x); }
  static double atan2(double y, double x) noexcept { return __builtin_atan2(y, x); }
  static double cbrt(double x) noexcept { return __builtin_cbrt(x); }
  static double ceil(double x) noexcept { return __builtin_ceil(x); }
  static double cos(double x) noexcept { return __builtin_cos(x); }
  static double cosh(double x) noexcept { return __builtin_cosh(x); }
  static double exp(double x) noexcept { return __builtin_exp(x); }
  static double floor(double x) noexcept { return __builtin_floor(x); }
  static double log(double x) noexcept { return __builtin_log(x); }
  static double log10(double x) noexcept { return __builtin_log10(x); }
  static double log2(double x) noexcept { return __builtin_log2(x); }
  static double pow(double x, double y) noexcept { return __builtin_pow(x, y); }
  static double round(double x) noexcept { return __builtin_floor(x + 0.5); }
  static double sin(double x) noexcept { return __builtin_sin(x); }
  static double sinh(double x) noexcept { return __builtin_sinh(x); }
  static double sqrt(double x) noexcept { return __builtin_sqrt(x); }
  static double tan(double x) noexcept { return __builtin_tan(x); }
  static double tanh(double x) noexcept { return __builtin_tanh(x); }
  static double trunc(double x) noexcept { return __builtin_trunc(x); }
  template <typename A, typename B>
  static constexpr auto max(A a, B b) noexcept { return a < b ? b : a; }
  template <typename A, typename B>
  static constexpr auto min(A a, B b) noexcept { return a < b ? a : b; }

  static constexpr double E = 2.718281828459045;
  static constexpr double LN10 = 2.302585092994046;
  static constexpr double LN2 = 0.6931471805599453;
  static constexpr double LOG10E = 0.4342944819032518;
  static constexpr double LOG2E = 1.4426950408889634;
  static constexpr double PI = 3.141592653589793;
  static constexpr double SQRT1_2 = 0.7071067811865476;
  static constexpr double SQRT2 = 1.4142135623730951;
};
inline constexpr math_t Math{};

template <typename V>
constexpr double value_of(V v) noexcept
{
  if constexpr(is_cint_<V>::value || is_int_<V>::value)
    return double(as_i32(v));
  else
    return double(v);
}

template <typename V>
[[gnu::always_inline]] inline void emit(double* hold, int mode, V v, double& l, double& r) noexcept
{
  const double x = value_of(v);
  mix(hold, mode, x, x, l, r);
}
}
)_";

enum class Tok
{
  Ident,
  Number,
  String,
  Punct,
  Newline
};

struct Token
{
  Tok kind;
  std::string text;
  int line;
};

struct Error
{
  std::string message;
  int line{-1};
  explicit operator bool() const noexcept { return !message.empty(); }
};

constexpr std::array js_puncts{
    ">>>=", ">>>", "===", "!==", "**=", "<<=", ">>=", "...", "**", "=>", "==", "!=",
    "<=",   ">=",  "&&",  "||",  "??",  "?.",  "++",  "--",  "+=", "-=", "*=", "/=",
    "%=",   "&=",  "|=",  "^=",  "<<",  ">>"};
constexpr std::array c_puncts{
    "<<=", ">>=", "...", "==", "!=", "<=", ">=", "&&", "||", "++", "--", "+=",
    "-=",  "*=",  "/=",  "%=", "&=", "|=", "^=", "<<", ">>", "->", "::"};

bool is_ident_start(char c) noexcept
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$'
         || (unsigned char)c >= 0x80;
}
bool is_digit(char c) noexcept
{
  return c >= '0' && c <= '9';
}
bool is_ident_char(char c) noexcept
{
  return is_ident_start(c) || is_digit(c);
}

Error lex(std::string_view s, Language lang, std::vector<Token>& out)
{
  const bool js = lang == Language::JavaScript;
  int line = 1;
  std::size_t i = 0;
  const std::size_t n = s.size();
  while(i < n)
  {
    const char c = s[i];
    if(c == '\n')
    {
      out.push_back({Tok::Newline, {}, line});
      ++line;
      ++i;
    }
    else if(c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v')
    {
      ++i;
    }
    else if(c == '/' && i + 1 < n && s[i + 1] == '/')
    {
      while(i < n && s[i] != '\n')
        ++i;
    }
    else if(c == '/' && i + 1 < n && s[i + 1] == '*')
    {
      const int start = line;
      i += 2;
      while(i + 1 < n && !(s[i] == '*' && s[i + 1] == '/'))
      {
        if(s[i] == '\n')
        {
          out.push_back({Tok::Newline, {}, line});
          ++line;
        }
        ++i;
      }
      if(i + 1 >= n)
        return {"unterminated comment", start};
      i += 2;
    }
    else if(is_digit(c) || (c == '.' && i + 1 < n && is_digit(s[i + 1])))
    {
      const std::size_t start = i;
      if(js)
      {
        if(c == '0' && i + 1 < n && std::string_view("xXbBoO").find(s[i + 1]) != std::string_view::npos)
        {
          i += 2;
          while(i < n && (std::isxdigit((unsigned char)s[i]) || s[i] == '_'))
            ++i;
        }
        else
        {
          while(i < n && (is_digit(s[i]) || s[i] == '_'))
            ++i;
          if(i < n && s[i] == '.')
          {
            ++i;
            while(i < n && (is_digit(s[i]) || s[i] == '_'))
              ++i;
          }
          if(i < n && (s[i] == 'e' || s[i] == 'E'))
          {
            std::size_t j = i + 1;
            if(j < n && (s[j] == '+' || s[j] == '-'))
              ++j;
            if(j < n && is_digit(s[j]))
            {
              i = j;
              while(i < n && (is_digit(s[i]) || s[i] == '_'))
                ++i;
            }
          }
        }
        if(i < n && is_ident_char(s[i]))
          return {"invalid number '" + std::string(s.substr(start, i + 1 - start)) + "'", line};
      }
      else
      {
        // pp-number
        ++i;
        while(i < n)
        {
          const char d = s[i];
          if((d == '+' || d == '-')
             && std::string_view("eEpP").find(s[i - 1]) != std::string_view::npos)
            ++i;
          else if(is_ident_char(d) || d == '.' || d == '\'')
            ++i;
          else
            break;
        }
      }
      out.push_back({Tok::Number, std::string(s.substr(start, i - start)), line});
    }
    else if(is_ident_start(c))
    {
      const std::size_t start = i;
      while(i < n && is_ident_char(s[i]))
        ++i;
      out.push_back({Tok::Ident, std::string(s.substr(start, i - start)), line});
    }
    else if(c == '"' || c == '\'' || (js && c == '`'))
    {
      const std::size_t start = i;
      const int start_line = line;
      ++i;
      while(i < n && s[i] != c)
      {
        if(s[i] == '\\' && i + 1 < n)
          ++i;
        else if(c == '`' && s[i] == '$' && i + 1 < n && s[i + 1] == '{')
          return {"template literals with ${} are not supported", line};
        if(s[i] == '\n')
        {
          if(c != '`')
            return {"unterminated string", start_line};
          ++line;
        }
        ++i;
      }
      if(i >= n)
        return {"unterminated string", start_line};
      ++i;
      out.push_back({Tok::String, std::string(s.substr(start, i - start)), start_line});
    }
    else
    {
      std::string_view rest = s.substr(i);
      std::string_view found;
      auto match = [&](const auto& list) {
        for(std::string_view p : list)
          if(rest.starts_with(p))
          {
            found = p;
            return;
          }
      };
      if(js)
        match(js_puncts);
      else
        match(c_puncts);

      if(found.empty())
      {
        if(js && std::string_view("+-*/%&|^~!<>=?:,.;()[]{}").find(c) == std::string_view::npos)
          return {std::string("unexpected character '") + c + "'", line};
        found = rest.substr(0, 1);
      }
      out.push_back({Tok::Punct, std::string(found), line});
      i += found.size();
    }
  }
  return {};
}

bool is_punct(const Token& t, std::string_view p) noexcept
{
  return t.kind == Tok::Punct && t.text == p;
}

bool ends_expression(const Token& t) noexcept
{
  if(t.kind != Tok::Punct)
    return true;
  return t.text == ")" || t.text == "]" || t.text == "}" || t.text == "++"
         || t.text == "--" || t.text == ";";
}

bool continues_expression(const Token& t) noexcept
{
  static const std::set<std::string_view> ops{
      "*",   "/",   "%",  "&",   "|",   "^",   "<<",  ">>",   ">>>", "<",  ">",
      "<=",  ">=",  "==", "!=",  "===", "!==", "&&",  "||",   "?",   ":",  ",",
      ")",   "]",   "}",  ".",   "=",   "**",  "??",  "+=",   "-=",  "*=", "/=",
      "%=",  "&=",  "|=", "^=",  "<<=", ">>=", ">>>=", "**=", "?.",  "->", "::"};
  return t.kind == Tok::Punct && ops.contains(t.text);
}

struct Formula
{
  std::vector<Token> tokens;
  int line{};
};

Error split(const std::vector<Token>& tokens, std::vector<Formula>& out)
{
  Formula cur;
  int depth = 0;
  bool line_break = false;
  auto flush = [&] {
    while(!cur.tokens.empty() && is_punct(cur.tokens.back(), ";"))
      cur.tokens.pop_back();
    if(!cur.tokens.empty())
      out.push_back(std::move(cur));
    cur = {};
  };

  for(const auto& t : tokens)
  {
    if(t.kind == Tok::Newline)
    {
      line_break = true;
      continue;
    }

    if(line_break && depth == 0 && !cur.tokens.empty()
       && ends_expression(cur.tokens.back()) && !continues_expression(t))
      flush();
    line_break = false;

    if(cur.tokens.empty())
      cur.line = t.line;
    if(t.kind == Tok::Punct)
    {
      if(t.text == "(" || t.text == "[" || t.text == "{")
        ++depth;
      else if(t.text == ")" || t.text == "]" || t.text == "}")
        depth = std::max(0, depth - 1);
    }
    cur.tokens.push_back(t);
  }
  flush();
  return {};
}

bool is_assignment(const Token& t) noexcept
{
  static const std::set<std::string_view> ops{
      "=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", "++", "--"};
  return t.kind == Tok::Punct && ops.contains(t.text);
}

const std::set<std::string_view>& js_math() noexcept
{
  static const std::set<std::string_view> names{
      "abs",   "acos",  "acosh", "asin",  "asinh", "atan",   "atanh", "atan2",
      "cbrt",  "ceil",  "clz32", "cos",   "cosh",  "exp",    "expm1", "floor",
      "fround", "hypot", "imul", "log",   "log1p", "log10",  "log2",  "max",
      "min",   "pow",   "random", "round", "sign", "sin",    "sinh",  "sqrt",
      "tan",   "tanh",  "trunc", "E",     "LN10",  "LN2",    "LOG10E", "LOG2E",
      "PI",    "SQRT1_2", "SQRT2"};
  return names;
}

const std::set<std::string_view>& js_keywords() noexcept
{
  static const std::set<std::string_view> names{
      "break",  "case",     "catch",  "class",  "const",   "continue", "debugger",
      "default", "delete",  "do",     "else",   "export",  "extends",  "finally",
      "for",    "function", "if",     "import", "in",      "instanceof", "let",
      "new",    "return",   "super",  "switch", "this",    "throw",    "try",
      "typeof", "var",      "void",   "while",  "with",    "yield",    "window",
      "async",  "await"};
  return names;
}

std::string mangle(std::string_view name)
{
  std::string r = "v_";
  for(char c : name)
  {
    if(c == '$')
      r += "_S";
    else if((unsigned char)c >= 0x80)
    {
      static constexpr char hex[] = "0123456789abcdef";
      r += "_x";
      r += hex[(unsigned char)c >> 4];
      r += hex[(unsigned char)c & 15];
    }
    else
      r += c;
  }
  return r;
}

std::string math_builtin(std::string_view name)
{
  if(name == "random")
    return "bb_S.random_num";
  return "js::" + std::string(name);
}

// Decode a JS string literal into UTF-16 code units.
Error decode_string(std::string_view lit, int line, std::u16string& out)
{
  const std::string_view s = lit.substr(1, lit.size() - 2);
  auto push_cp = [&](char32_t cp) {
    if(cp >= 0x10000)
    {
      cp -= 0x10000;
      out.push_back(char16_t(0xD800 + (cp >> 10)));
      out.push_back(char16_t(0xDC00 + (cp & 0x3FF)));
    }
    else
      out.push_back(char16_t(cp));
  };
  auto hex = [](char c) -> int {
    if(c >= '0' && c <= '9')
      return c - '0';
    if(c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    if(c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    return -1;
  };
  auto read_hex = [&](std::size_t& i, int count, char32_t& v) {
    v = 0;
    for(int k = 0; k < count; k++)
    {
      if(i >= s.size() || hex(s[i]) < 0)
        return false;
      v = v * 16 + hex(s[i++]);
    }
    return true;
  };

  std::size_t i = 0;
  while(i < s.size())
  {
    const unsigned char c = s[i];
    if(c == '\\' && i + 1 < s.size())
    {
      const char e = s[i + 1];
      i += 2;
      switch(e)
      {
        case 'n': push_cp('\n'); break;
        case 't': push_cp('\t'); break;
        case 'r': push_cp('\r'); break;
        case 'b': push_cp('\b'); break;
        case 'f': push_cp('\f'); break;
        case 'v': push_cp('\v'); break;
        case '0':
          if(i < s.size() && is_digit(s[i]))
            return {"octal escapes are not supported", line};
          push_cp(0);
          break;
        case '\r':
          if(i < s.size() && s[i] == '\n')
            ++i;
          break;
        case '\n': break;
        case 'x': {
          char32_t v;
          if(!read_hex(i, 2, v))
            return {"invalid \\x escape", line};
          push_cp(v);
          break;
        }
        case 'u': {
          char32_t v = 0;
          if(i < s.size() && s[i] == '{')
          {
            ++i;
            while(i < s.size() && s[i] != '}')
            {
              if(hex(s[i]) < 0)
                return {"invalid \\u escape", line};
              v = v * 16 + hex(s[i++]);
            }
            if(i >= s.size() || v > 0x10FFFF)
              return {"invalid \\u escape", line};
            ++i;
            push_cp(v);
          }
          else
          {
            if(!read_hex(i, 4, v))
              return {"invalid \\u escape", line};
            out.push_back(char16_t(v));
          }
          break;
        }
        default:
          push_cp((unsigned char)e);
          break;
      }
    }
    else if(c < 0x80)
    {
      push_cp(c);
      ++i;
    }
    else
    {
      // UTF-8
      int len = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : (c >= 0xC0) ? 2 : 1;
      char32_t cp = len == 4 ? (c & 0x07) : len == 3 ? (c & 0x0F) : len == 2 ? (c & 0x1F) : c;
      for(int k = 1; k < len && i + k < s.size(); k++)
        cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
      push_cp(cp);
      i += len;
    }
  }
  return {};
}

std::string js_number(std::string text)
{
  std::erase(text, '_');
  if(text.size() > 2 && text[0] == '0' && (text[1] == 'o' || text[1] == 'O'))
  {
    unsigned long long v = 0;
    for(std::size_t i = 2; i < text.size(); i++)
      v = v * 8 + (text[i] - '0');
    text = std::to_string(v) + ".";
  }
  else
  {
    const bool prefixed = text.size() > 2 && text[0] == '0'
                          && std::string_view("xXbB").find(text[1]) != std::string_view::npos;
    // As a double literal: an integer literal may not fit in any C++ integer type
    if(!prefixed && text.find_first_of(".eE") == std::string::npos)
      text += ".";
  }
  return "js::num(" + text + ")";
}

Error translate_js(
    const Formula& f, const std::set<std::string>& vars, std::string& out)
{
  const auto& toks = f.tokens;
  std::vector<char> brackets;
  const Token* prev = nullptr;
  auto emit = [&](std::string_view s) {
    out += s;
    out += ' ';
  };

  for(std::size_t i = 0; i < toks.size(); i++)
  {
    const Token& t = toks[i];
    switch(t.kind)
    {
      case Tok::Newline:
        break;
      case Tok::Number:
        emit(js_number(t.text));
        break;
      case Tok::String: {
        std::u16string units;
        if(auto err = decode_string(t.text, t.line, units))
          return err;
        std::string lit = "js::str(u\"";
        for(char16_t u : units)
        {
          static constexpr char hex[] = "0123456789abcdef";
          lit += "\\x";
          lit += hex[(u >> 12) & 15];
          lit += hex[(u >> 8) & 15];
          lit += hex[(u >> 4) & 15];
          lit += hex[u & 15];
        }
        lit += "\")";
        emit(lit);
        break;
      }
      case Tok::Ident: {
        const std::string& name = t.text;
        if(prev && is_punct(*prev, "."))
        {
          emit(name);
        }
        else if(vars.contains(name))
        {
          emit("bb_S." + mangle(name));
        }
        else if(name == "t")
        {
          emit("t");
        }
        else if(name == "Math")
        {
          if(i + 2 >= toks.size() || !is_punct(toks[i + 1], ".")
             || toks[i + 2].kind != Tok::Ident)
            return {"Math must be followed by a function or a constant", t.line};
          const std::string& member = toks[i + 2].text;
          if(!js_math().contains(member))
            return {"Math." + member + " is not supported", t.line};
          emit(math_builtin(member));
          i += 2;
        }
        else if(js_math().contains(name))
        {
          emit(math_builtin(name));
        }
        else if(name == "int")
        {
          emit("js::int_");
        }
        else if(name == "true")
        {
          emit("js::num(1.)");
        }
        else if(name == "false" || name == "null")
        {
          emit("js::num(0.)");
        }
        else if(name == "undefined" || name == "NaN")
        {
          emit("js::num(js::nan_v)");
        }
        else if(name == "Infinity")
        {
          emit("js::num(js::inf_v)");
        }
        else if(js_keywords().contains(name))
        {
          return {"'" + name + "' is not supported in a bytebeat expression", t.line};
        }
        else
        {
          return {"'" + name + "' is not defined", t.line};
        }
        break;
      }
      case Tok::Punct: {
        const std::string& p = t.text;
        if(p == "===")
          emit("==");
        else if(p == "!==")
          emit("!=");
        else if(p == ">>>")
          emit(">> js::ushr_t{} >>");
        else if(
            p == ">>>=" || p == "**" || p == "**=" || p == "=>" || p == "??" || p == "?."
            || p == "..." || p == "{" || p == "}")
          return {"'" + p + "' is not supported", t.line};
        else if(p == ";")
          return {"';' is not supported: separate expressions with ','", t.line};
        else if(p == "(")
        {
          brackets.push_back('(');
          emit("(");
        }
        else if(p == ")")
        {
          if(brackets.empty() || brackets.back() != '(')
            return {"unbalanced ')'", t.line};
          brackets.pop_back();
          emit(")");
        }
        else if(p == "[")
        {
          const bool subscript
              = prev
                && (prev->kind == Tok::Ident || prev->kind == Tok::Number
                    || prev->kind == Tok::String || is_punct(*prev, ")")
                    || is_punct(*prev, "]"));
          brackets.push_back(subscript ? '[' : 'a');
          emit(subscript ? "[" : "js::arr(");
        }
        else if(p == "]")
        {
          if(brackets.empty() || (brackets.back() != '[' && brackets.back() != 'a'))
            return {"unbalanced ']'", t.line};
          emit(brackets.back() == 'a' ? ")" : "]");
          brackets.pop_back();
        }
        else
          emit(p);
        break;
      }
    }
    prev = &toks[i];
  }
  if(!brackets.empty())
    return {"unbalanced brackets", f.line};
  return {};
}

void collect_assigned(const std::vector<Formula>& formulas, std::set<std::string>& vars)
{
  for(const auto& f : formulas)
  {
    const auto& toks = f.tokens;
    for(std::size_t i = 0; i < toks.size(); i++)
    {
      if(toks[i].kind != Tok::Ident || toks[i].text == "t")
        continue;
      if(i > 0 && is_punct(toks[i - 1], "."))
        continue;
      const bool assigned_after = i + 1 < toks.size() && is_assignment(toks[i + 1]);
      const bool incremented_before
          = i > 0 && (is_punct(toks[i - 1], "++") || is_punct(toks[i - 1], "--"));
      if(assigned_after || incremented_before)
        vars.insert(toks[i].text);
    }
  }
}
}

Generated generate(std::string_view source, Language lang)
{
  Generated res;
  const bool js = lang == Language::JavaScript;

  std::vector<Token> tokens;
  if(auto err = lex(source, lang, tokens))
  {
    res.error = err.message;
    res.error_line = err.line;
    return res;
  }

  std::vector<Formula> formulas;
  split(tokens, formulas);

  std::set<std::string> vars;
  std::vector<std::string> exprs;
  if(js)
  {
    collect_assigned(formulas, vars);
    for(const auto& f : formulas)
    {
      std::string e;
      if(auto err = translate_js(f, vars, e))
      {
        res.error = err.message;
        res.error_line = err.line;
        return res;
      }
      exprs.push_back(std::move(e));
    }
  }
  else
  {
    for(const auto& f : formulas)
    {
      std::string e;
      for(const auto& t : f.tokens)
      {
        e += t.text;
        e += ' ';
      }
      exprs.push_back(std::move(e));
    }
  }

  const int k = int(exprs.size());
  res.formulas = k;

  std::string& code = res.code;
  code.reserve(runtime_common.size() + runtime_js.size() + 2048);
  auto out = std::back_inserter(code);
  code += runtime_common;
  code += js ? runtime_js : runtime_c;

  code += "namespace {\nstruct bb_state : bb_rt::state_base\n{\n";
  fmt::format_to(out, "  double hold[{}][2];\n", std::max(k, 1));
  for(const auto& v : vars)
    fmt::format_to(out, "  js::num {};\n", mangle(v));
  if(js)
    code += "  js::num random_num() noexcept { return random(); }\n";
  code += "};\n}\n";

  code += R"_(
extern "C" int score_bytebeat(void* bb_sp, double* bb_L, double* bb_R, const double* bb_T, int bb_n, int bb_mode)
{
  if(!bb_sp)
    return sizeof(bb_state);
  bb_state& bb_S = *static_cast<bb_state*>(bb_sp);
  for(int bb_i = 0; bb_i < bb_n; bb_i++)
  {
    if(!bb_S.has_last || bb_T[bb_i] != bb_S.last_t)
    {
      bb_S.has_last = 1;
      bb_S.last_t = bb_T[bb_i];
)_";
  code += js ? "      js::num t{bb_T[bb_i]};\n"
             : "      c::c_int t{c::as_i32((long long)bb_T[bb_i])};\n";
  if(!js)
    code += "      using namespace c;\n";
  code += "      (void)t;\n      double bb_l = 0., bb_r = 0.;\n";

  for(int i = 0; i < k; i++)
  {
    fmt::format_to(
        out,
        "#line {} \"bytebeat\"\n"
        "      {}::emit(bb_S.hold[{}], bb_mode, (({})), bb_l, bb_r);\n",
        formulas[i].line, js ? "js" : "c", i, exprs[i]);
  }

  const double gain = k > 0 ? 1. / k : 0.;
  fmt::format_to(
      out,
      "      bb_S.out[0] = bb_l * {0};\n"
      "      bb_S.out[1] = bb_r * {0};\n",
      gain);
  code += R"_(    }
    bb_L[bb_i] = bb_S.out[0];
    bb_R[bb_i] = bb_S.out[1];
  }
  return sizeof(bb_state);
}
)_";
  return res;
}
}
