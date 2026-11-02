#pragma once
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

/**
 * Reading the elements of a raw buffer as floats: what the bytes are, how
 * many components an element has, and where the elements are. Nothing of
 * this travels with a GPU buffer, the user says it.
 */
namespace Ui::Monitor::Buffer
{
enum class ElementType : int
{
  Float,
  Half,
  Int,
  UInt,
  //! 0 to 255 read as 0 to 1
  UNorm8,
  UInt8
};

struct Layout
{
  ElementType type{ElementType::Float};
  //! 1 to 4
  int components{1};
  //! Bytes from the start of an element to the start of the next; 0 when
  //! the elements follow each other.
  int64_t stride{};
  //! Bytes before the first element
  int64_t offset{};

  bool operator==(const Layout&) const noexcept = default;
};

constexpr int componentBytes(ElementType t) noexcept
{
  switch(t)
  {
    case ElementType::Half:
      return 2;
    case ElementType::UNorm8:
    case ElementType::UInt8:
      return 1;
    default:
      return 4;
  }
}

constexpr int components(const Layout& l) noexcept
{
  return std::clamp(l.components, 1, 4);
}

//! Bytes one element covers
constexpr int64_t elementBytes(const Layout& l) noexcept
{
  return int64_t(componentBytes(l.type)) * components(l);
}

constexpr int64_t strideOf(const Layout& l) noexcept
{
  return l.stride > 0 ? l.stride : elementBytes(l);
}

//! Whole elements in `bytes` bytes, the first at the layout's offset.
constexpr int64_t elementCount(const Layout& l, int64_t bytes) noexcept
{
  const int64_t room = bytes - std::max<int64_t>(l.offset, 0);
  const int64_t e = elementBytes(l);
  if(room < e)
    return 0;
  return (room - e) / strideOf(l) + 1;
}

//! Bytes one readback holds at most, whatever the layout and the controls
inline constexpr int64_t max_readback_bytes = int64_t(16) << 20;

//! Elements a view reads at most
inline constexpr int64_t max_view_elements = int64_t(1) << 20;

//! Which elements of a buffer a view reads.
struct Sampling
{
  //! Index of the first element read
  int64_t first{};
  //! Elements from one read to the next: 1 reads them all
  int64_t step{1};
  //! Elements read
  int64_t count{};
  //! Elements from the first to the end of the buffer
  int64_t window{};
  //! Whether the bytes a readback holds kept `count` below what was asked
  bool capped{};

  bool operator==(const Sampling&) const noexcept = default;
};

/**
 * The elements to read of `available`, from `first`: as many as `max_elements`
 * and `cap_bytes` allow. With `overview`, a window holding more than that is
 * read at an even step over its whole length instead of from its start.
 */
constexpr Sampling sampleElements(
    int64_t available, int64_t first, int64_t max_elements, int64_t element_bytes,
    int64_t cap_bytes, bool overview) noexcept
{
  Sampling s;
  s.first = std::clamp<int64_t>(first, 0, std::max<int64_t>(available, 0));
  s.window = std::max<int64_t>(available, 0) - s.first;
  if(s.window <= 0)
    return s;
  const int64_t fit = std::max<int64_t>(1, cap_bytes / std::max<int64_t>(element_bytes, 1));
  const int64_t asked = std::max<int64_t>(1, max_elements);
  const int64_t limit = std::min(asked, fit);
  s.capped = fit < asked && s.window > fit;
  if(overview && s.window > limit)
  {
    s.step = (s.window + limit - 1) / limit;
    s.count = (s.window + s.step - 1) / s.step;
  }
  else
    s.count = std::min(s.window, limit);
  return s;
}

/**
 * Elements, `stride` bytes apart, that one contiguous read of at most
 * `cap_bytes` holds when it starts on a 4-byte boundary `misalign` bytes
 * before the first and ends on one too.
 */
constexpr int64_t contiguousFit(
    int64_t stride, int64_t element_bytes, int64_t misalign, int64_t cap_bytes) noexcept
{
  const int64_t room = cap_bytes - element_bytes - misalign - 3;
  if(room < 0)
    return 0;
  return room / std::max<int64_t>(stride, 1) + 1;
}

//! Whether the values are integers, to print them as such.
constexpr bool integral(ElementType t) noexcept
{
  return t == ElementType::Int || t == ElementType::UInt || t == ElementType::UInt8;
}

//! IEEE 754 binary16 to binary32, subnormals, infinities and NaN included.
inline float halfToFloat(uint16_t h) noexcept
{
  const uint32_t sign = uint32_t(h & 0x8000u) << 16;
  const uint32_t exponent = (h >> 10) & 0x1fu;
  const uint32_t mantissa = h & 0x3ffu;
  if(exponent == 0)
  {
    const float f = std::ldexp(float(mantissa), -24);
    return sign ? -f : f;
  }
  if(exponent == 31)
    return std::bit_cast<float>(sign | 0x7f800000u | (mantissa << 13));
  return std::bit_cast<float>(sign | ((exponent + 112u) << 23) | (mantissa << 13));
}

//! One component at `p`, which needs no alignment.
inline float readComponent(ElementType t, const unsigned char* p) noexcept
{
  switch(t)
  {
    case ElementType::Float: {
      float v;
      std::memcpy(&v, p, 4);
      return v;
    }
    case ElementType::Half: {
      uint16_t v;
      std::memcpy(&v, p, 2);
      return halfToFloat(v);
    }
    case ElementType::Int: {
      int32_t v;
      std::memcpy(&v, p, 4);
      return float(v);
    }
    case ElementType::UInt: {
      uint32_t v;
      std::memcpy(&v, p, 4);
      return float(v);
    }
    case ElementType::UNorm8:
      return *p / 255.f;
    case ElementType::UInt8:
      return float(*p);
  }
  return 0.f;
}

/**
 * The elements of `bytes`, the first at the layout's offset, into `out`:
 * the components of each element one after the other. Decodes as many
 * elements as both hold, and returns how many.
 */
inline int64_t
decode(const Layout& l, std::span<const unsigned char> bytes, std::span<float> out) noexcept
{
  const int n = components(l);
  const int cb = componentBytes(l.type);
  const int64_t stride = strideOf(l);
  const int64_t count = std::min(
      elementCount(l, int64_t(bytes.size())), int64_t(out.size()) / n);
  const unsigned char* src = bytes.data() + std::max<int64_t>(l.offset, 0);
  float* dst = out.data();
  for(int64_t i = 0; i < count; i++, src += stride)
    for(int c = 0; c < n; c++)
      *dst++ = readComponent(l.type, src + c * cb);
  return count;
}
}
