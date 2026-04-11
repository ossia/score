#include <score/config.hpp>

#include <ossia/detail/config.hpp>

#include <cstdint>
#include <cstring>
#include <string_view>

namespace score
{
#if !defined(_MSC_VER)
__attribute__((target("default")))
#endif
bool fast_contains(std::string_view data, std::string_view pattern)
{
  return data.contains(pattern);
}
}

#if defined(__x86_64__) && !defined(_MSC_VER) && __has_include(<immintrin.h>)

#pragma GCC push_options
#pragma GCC target("avx2")

#include <immintrin.h>
namespace
{
struct NeedleCtxShort
{
  size_t len;
  __m256i vfirst;
  __m256i vlast;
  __m256i vneedle;
  uint32_t len_mask;
};

__attribute__((target("avx2"))) NeedleCtxShort
prepare_needle_short(const void* needle, size_t len)
{
  auto* p = (const uint8_t*)needle;
  NeedleCtxShort ctx;
  ctx.len = len;
  ctx.vfirst = _mm256_set1_epi8(p[0]);
  ctx.vlast = _mm256_set1_epi8(p[len - 1]);

  uint8_t padded[32] = {0};
  std::memcpy(padded, p, len);

  ctx.vneedle = _mm256_loadu_si256((const __m256i*)padded);

  ctx.len_mask = (1ULL << len) - 1;

  return ctx;
}

__attribute__((target("avx2"))) bool
avx2_contains_short(const char* data, size_t size, const NeedleCtxShort& ctx)
{
  if(size < ctx.len)
    return false;
  const size_t scan_end = size - ctx.len;
  size_t i = 0;

  for(; i + 63 + ctx.len - 1 <= size; i += 64)
  {
    __m256i b0_f = _mm256_load_si256((const __m256i*)(data + i));
    __m256i b1_f = _mm256_load_si256((const __m256i*)(data + i + 32));

    __m256i b0_l = _mm256_loadu_si256((const __m256i*)(data + i + ctx.len - 1));
    __m256i b1_l = _mm256_loadu_si256((const __m256i*)(data + i + 32 + ctx.len - 1));

    __m256i eq0 = _mm256_and_si256(
        _mm256_cmpeq_epi8(b0_f, ctx.vfirst), _mm256_cmpeq_epi8(b0_l, ctx.vlast));
    __m256i eq1 = _mm256_and_si256(
        _mm256_cmpeq_epi8(b1_f, ctx.vfirst), _mm256_cmpeq_epi8(b1_l, ctx.vlast));

    uint32_t mask0 = _mm256_movemask_epi8(eq0);
    uint32_t mask1 = _mm256_movemask_epi8(eq1);

    if(mask0 | mask1)
    {
      while(mask0)
      {
        uint32_t bit = __builtin_ctz(mask0);

        __m256i candidate = _mm256_loadu_si256((const __m256i*)(data + i + bit));

        uint32_t cmp_mask
            = _mm256_movemask_epi8(_mm256_cmpeq_epi8(candidate, ctx.vneedle));

        if((cmp_mask & ctx.len_mask) == ctx.len_mask)
          return true;

        mask0 &= mask0 - 1;
      }
      while(mask1)
      {
        uint32_t bit = __builtin_ctz(mask1);

        __m256i candidate = _mm256_loadu_si256((const __m256i*)(data + i + 32 + bit));
        uint32_t cmp_mask
            = _mm256_movemask_epi8(_mm256_cmpeq_epi8(candidate, ctx.vneedle));
        if((cmp_mask & ctx.len_mask) == ctx.len_mask)
          return true;

        mask1 &= mask1 - 1;
      }
    }
  }

  // Because we padded the buffer safely, we can use the exact same logic for the tail end!
  for(; i <= scan_end; ++i)
  {
    if(data[i] == ctx.vneedle[0] && data[i + ctx.len - 1] == ctx.vneedle[ctx.len - 1])
    {
      __m256i candidate = _mm256_loadu_si256((const __m256i*)(data + i));
      uint32_t cmp_mask
          = _mm256_movemask_epi8(_mm256_cmpeq_epi8(candidate, ctx.vneedle));
      if((cmp_mask & ctx.len_mask) == ctx.len_mask)
        return true;
    }
  }
  return false;
}

struct NeedleCtx
{
  const uint8_t* needle{};
  size_t len{};
  __m256i vfirst{};
  __m256i vlast{};
};

__attribute__((target("avx2"))) NeedleCtx prepare_needle(const void* needle, size_t len)
{
  auto* p = (const uint8_t*)needle;
  return {p, len, _mm256_set1_epi8(p[0]), _mm256_set1_epi8(p[len - 1])};
}

__attribute__((target("avx2"))) bool
avx2_contains(const char* data, size_t size, NeedleCtx ctx)
{
  if(size < ctx.len)
    return false;
  const size_t scan_end = size - ctx.len;
  size_t i = 0;

  for(; i + 63 + ctx.len - 1 <= size; i += 64)
  {
    __m256i b0_f = _mm256_load_si256((const __m256i*)(data + i));
    __m256i b1_f = _mm256_load_si256((const __m256i*)(data + i + 32));

    __m256i b0_l = _mm256_loadu_si256((const __m256i*)(data + i + ctx.len - 1));
    __m256i b1_l = _mm256_loadu_si256((const __m256i*)(data + i + 32 + ctx.len - 1));

    __m256i eq0 = _mm256_and_si256(
        _mm256_cmpeq_epi8(b0_f, ctx.vfirst), _mm256_cmpeq_epi8(b0_l, ctx.vlast));
    __m256i eq1 = _mm256_and_si256(
        _mm256_cmpeq_epi8(b1_f, ctx.vfirst), _mm256_cmpeq_epi8(b1_l, ctx.vlast));

    uint32_t mask0 = _mm256_movemask_epi8(eq0);
    uint32_t mask1 = _mm256_movemask_epi8(eq1);

    if(mask0 | mask1)
    {
      while(mask0)
      {
        uint32_t bit = __builtin_ctz(mask0);
        if(ctx.len <= 2 || memcmp(data + i + bit + 1, ctx.needle + 1, ctx.len - 2) == 0)
          return true;
        mask0 &= mask0 - 1;
      }
      while(mask1)
      {
        uint32_t bit = __builtin_ctz(mask1);
        if(ctx.len <= 2
           || memcmp(data + i + 32 + bit + 1, ctx.needle + 1, ctx.len - 2) == 0)
          return true;
        mask1 &= mask1 - 1;
      }
    }
  }

  // Fallback for remaining bytes
  for(; i <= scan_end; ++i)
  {
    if(data[i] == ctx.needle[0] && data[i + ctx.len - 1] == ctx.needle[ctx.len - 1]
       && (ctx.len <= 2 || memcmp(data + i + 1, ctx.needle + 1, ctx.len - 2) == 0))
      return true;
  }
  return false;
}
}

namespace score
{
__attribute__((target("avx2"))) bool
fast_contains(std::string_view data, std::string_view pattern)
{
  if(pattern.size() < 32)
  {
    [[likely]];
    const auto n = prepare_needle_short(pattern.data(), pattern.size());
    return avx2_contains_short(data.data(), data.size(), n);
  }
  else
  {
    const auto n = prepare_needle(pattern.data(), pattern.size());
    return avx2_contains(data.data(), data.size(), n);
  }
}
}
#pragma GCC pop_options
#endif