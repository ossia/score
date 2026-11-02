#pragma once
#include <Ui/Monitor/BufferDecode.hpp>

#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <span>

/**
 * The gather pass of the buffer view: a compute shader copies the elements a
 * view reads, wherever they are in the buffer, one after the other into a
 * staging buffer, and that alone is read back. What it reads back is the
 * size of the elements, not of the bytes between them.
 *
 * In an overview it also measures every element of the window: the extent
 * of each component and how many are not finite, at the head of the staging
 * buffer.
 *
 * This holds what the CPU computes for it, and the same arithmetic as the
 * shader, which the tests check it against.
 */
namespace Ui::Monitor::Buffer
{
//! Why a buffer is read back as one range instead of through the gather pass.
enum class Fallback : uint8_t
{
  None,
  NoCompute,
  NotStorage,
  TooLarge,
  Failed
};

//! The uniforms of the gather shader, as std140 lays them out.
struct GatherParams
{
  //! Byte of the first element read, from the start of the bound range
  uint32_t base{};
  //! Bytes from one element read to the next
  uint32_t sample_stride{};
  uint32_t element_bytes{};
  //! Words of elements written
  uint32_t out_words{};
  //! Words of the bound range
  uint32_t src_words{};
  //! Elements measured; 0 measures none
  uint32_t reduce_count{};
  //! Byte of the first element measured, from the start of the bound range
  uint32_t reduce_base{};
  //! Bytes from one element measured to the next
  uint32_t reduce_stride{};
  uint32_t type{};
  uint32_t components{};
  uint32_t component_bytes{};
  uint32_t pad{};
};
static_assert(sizeof(GatherParams) == 48);

//! Words before the elements in the staging buffer: the minimum of each
//! component, their maximum, then the count of values not finite
inline constexpr int gather_header_words = 16;
inline constexpr int64_t gather_header_bytes = gather_header_words * 4;
inline constexpr int gather_local_size = 256;
//! Workgroups of a dispatch at most: each invocation goes over as many words
//! and elements as it takes
inline constexpr int64_t gather_max_groups = 4096;
//! Elements one pass can measure: an invocation's index never wraps
inline constexpr int64_t gather_max_reduce
    = int64_t(std::numeric_limits<uint32_t>::max())
      - gather_max_groups * gather_local_size;
//! Alignment of the start of the bound range: the largest storage buffer
//! offset alignment of the backends
inline constexpr int64_t gather_bind_alignment = 256;

//! Bytes of a buffer of `buffer_bytes` the shader can read: it reads words,
//! and the bytes of a last word cut short are past the end of its array
constexpr int64_t gatherReadable(int64_t buffer_bytes) noexcept
{
  return buffer_bytes & ~int64_t(3);
}

//! What a gather pass reads and where its result goes.
struct GatherPlan
{
  GatherParams params{};
  //! The range of the source buffer the shader reads
  int64_t bind_offset{};
  int64_t bind_size{};
  //! Bytes the staging buffer needs: the header and the elements
  int64_t staging_bytes{};
  //! The range of the staging buffer read back
  int64_t readback_offset{};
  int64_t readback_size{};
  int groups_x{};
  int groups_y{};
  bool valid{};
};

/**
 * The gather pass of the sampled elements `s` of a buffer of `buffer_bytes`
 * bytes, element 0 at byte `origin`. With `reduce`, it also measures every
 * element of the window. Not valid when the range it reads is larger than
 * `max_bind` bytes, past the 32 bit offsets of the shader or past what the
 * shader can read of the buffer.
 */
constexpr GatherPlan planGather(
    const Layout& l, int64_t origin, int64_t buffer_bytes, const Sampling& s, bool reduce,
    int64_t max_bind, int max_groups_per_dim) noexcept
{
  GatherPlan p;
  if(s.count <= 0 || origin < 0)
    return p;
  constexpr int64_t u32 = std::numeric_limits<uint32_t>::max();
  const int64_t e = elementBytes(l);
  const int64_t stride = strideOf(l);
  const int64_t start = origin + s.first * stride;
  const int64_t sample_stride = s.count > 1 ? s.step * stride : 0;
  int64_t end = start + (s.count - 1) * sample_stride + e;
  const int64_t reduce_count = reduce ? s.window : 0;
  if(reduce_count > gather_max_reduce)
    return p;
  if(reduce)
    end = std::max(end, start + (reduce_count - 1) * stride + e);
  const int64_t readable = gatherReadable(buffer_bytes);
  if(end > readable || buffer_bytes > u32 || sample_stride > u32)
    return p;

  p.bind_offset = start / gather_bind_alignment * gather_bind_alignment;
  p.bind_size = std::min(readable, (end + 3) / 4 * 4) - p.bind_offset;
  if(p.bind_size > max_bind)
    return p;

  const int64_t out_words = (s.count * e + 3) / 4;
  auto& g = p.params;
  g.base = uint32_t(start - p.bind_offset);
  g.sample_stride = uint32_t(sample_stride);
  g.element_bytes = uint32_t(e);
  g.out_words = uint32_t(out_words);
  g.src_words = uint32_t(p.bind_size / 4);
  g.reduce_count = uint32_t(reduce_count);
  g.reduce_base = g.base;
  g.reduce_stride = uint32_t(stride);
  g.type = uint32_t(l.type);
  g.components = uint32_t(components(l));
  g.component_bytes = uint32_t(componentBytes(l.type));

  p.staging_bytes = gather_header_bytes + out_words * 4;
  p.readback_offset = reduce ? 0 : gather_header_bytes;
  p.readback_size = p.staging_bytes - p.readback_offset;

  // Wider than a dimension allows: the rows of groups go on y
  const int64_t invocations = std::max(out_words, reduce_count);
  const int64_t groups = std::clamp<int64_t>(
      (invocations + gather_local_size - 1) / gather_local_size, 1, gather_max_groups);
  const int64_t max_dim = std::max(1, max_groups_per_dim);
  p.groups_x = int(std::min(groups, max_dim));
  p.groups_y = int((groups + p.groups_x - 1) / p.groups_x);
  p.valid = true;
  return p;
}

//! The `n` bytes (1 to 4) at byte `b` of `words`, little endian; a word past
//! the end reads as 0. The shader's bytesAt.
constexpr uint32_t gatherBytesAt(std::span<const uint32_t> words, uint32_t b, uint32_t n) noexcept
{
  auto word = [&](uint32_t w) { return w < words.size() ? words[w] : 0u; };
  const uint32_t w = b >> 2;
  const uint32_t shift = (b & 3u) * 8u;
  uint32_t v = word(w) >> shift;
  if(shift + n * 8u > 32u)
    v |= word(w + 1u) << (32u - shift);
  return n == 4u ? v : (v & ((1u << (n * 8u)) - 1u));
}

//! The word `w` of the elements the shader writes. The shader's gathered.
constexpr uint32_t
gatherWord(const GatherParams& g, std::span<const uint32_t> words, uint32_t w) noexcept
{
  const uint32_t b = w * 4u;
  if(g.element_bytes % 4u == 0u)
    return gatherBytesAt(
        words, g.base + (b / g.element_bytes) * g.sample_stride + b % g.element_bytes, 4u);
  uint32_t v = 0;
  for(uint32_t i = 0; i < 4u; i++)
  {
    const uint32_t at = b + i;
    v |= gatherBytesAt(
             words, g.base + (at / g.element_bytes) * g.sample_stride + at % g.element_bytes,
             1u)
         << (i * 8u);
  }
  return v;
}

//! A float as an unsigned integer in the same order, for atomic min and max.
constexpr uint32_t orderedFloat(float f) noexcept
{
  const uint32_t u = std::bit_cast<uint32_t>(f);
  return (u & 0x80000000u) ? ~u : (u | 0x80000000u);
}

constexpr float unorderedFloat(uint32_t u) noexcept
{
  return std::bit_cast<float>((u & 0x80000000u) ? (u & 0x7fffffffu) : ~u);
}

//! What the header of a staging buffer says of the elements measured.
struct GatherRange
{
  //! Empty, lo above hi, for a component without one finite value
  std::array<float, 4> lo{}, hi{};
  int64_t non_finite{};
};

inline GatherRange readGatherHeader(std::span<const uint32_t> header, int components) noexcept
{
  GatherRange r;
  r.lo.fill(std::numeric_limits<float>::max());
  r.hi.fill(std::numeric_limits<float>::lowest());
  if(header.size() < std::size_t(gather_header_words))
    return r;
  for(int c = 0; c < std::clamp(components, 1, 4); c++)
  {
    // The values the header is reset to: nothing finite was found
    if(header[c] == 0xffffffffu && header[4 + c] == 0u)
      continue;
    r.lo[c] = unorderedFloat(header[c]);
    r.hi[c] = unorderedFloat(header[4 + c]);
  }
  r.non_finite = header[8];
  return r;
}
}
