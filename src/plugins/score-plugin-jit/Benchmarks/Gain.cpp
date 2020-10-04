#include "Gain.hpp"

#include <immintrin.h>

#include <vector>

void gain(
    const std::vector<aligned_vec>& p1,
    float g,
    std::vector<aligned_vec>& p2)
{
  const auto chans = p1.size();
  for (std::size_t i = 0; i < chans; i++)
  {
    auto& in = p1[i];
    auto& out = p2[i];

    const auto samples = in.size();

    std::size_t j = 0;

#if defined(__AVX2__) && __has_include(<immintrin.h>)
    if (samples > 4)
    {
      const auto gain = _mm256_set1_pd(g);
      for (; j < samples - 4; j += 4)
      {
        _mm256_store_pd(&out[j], _mm256_mul_pd(_mm256_load_pd(&in[j]), gain));
      }
    }
#endif

    {
      for (; j < samples; j++)
      {
        out[j] = in[j] * g;
      }
    }
  }
}
