// GPUBufferScatter::scatterOnCpu, the decoding a CSF geometry attribute gets
// on a backend without compute, matches the scatter shader's: unorm bytes,
// halves and integers read as the declared float / uint / int, missing
// components (0, 0, 0, 1), elements at their std430 stride (shader board C40).
// The shader itself is pinned by test_gfx_csf_packed_attribute.
#include <Gfx/Graph/GPUBufferScatter.hpp>

#include <QFloat16>

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

using score::gfx::GPUBufferScatter;
using D = GPUBufferScatter::DestinationKind;
using S = GPUBufferScatter::SourceFormat;

namespace
{
std::vector<uint32_t> scatter(const GPUBufferScatter::Params& p, const std::vector<unsigned char>& src)
{
  const uint32_t stride = p.dst_stride_floats ? p.dst_stride_floats : p.dst_components;
  std::vector<uint32_t> dst(p.element_count * stride, 0xDEADBEEF);
  GPUBufferScatter::scatterOnCpu(
      p, reinterpret_cast<const char*>(src.data()), (int64_t)src.size(),
      reinterpret_cast<char*>(dst.data()), (int64_t)dst.size() * 4);
  return dst;
}

float f(uint32_t bits)
{
  float v;
  std::memcpy(&v, &bits, 4);
  return v;
}
}

TEST_CASE("scatterOnCpu decodes unorm bytes, at an unaligned offset", "[gfx][scatter][C40]")
{
  // Two elements, 5 bytes apart, starting at byte 3.
  const std::vector<unsigned char> src{0, 0, 0, 51, 102, 255, 7, 0, 0, 255, 0};
  GPUBufferScatter::Params p{
      .element_count = 2, .src_components = 2, .dst_components = 4,
      .src_stride_bytes = 5, .src_offset_bytes = 3, .src_format = S::Unorm8};
  const auto d = scatter(p, src);
  CHECK(f(d[0]) == 51.f / 255.f);
  CHECK(f(d[1]) == 102.f / 255.f);
  CHECK(f(d[2]) == 0.f);
  CHECK(f(d[3]) == 1.f);
  CHECK(f(d[4]) == 0.f);
  CHECK(f(d[5]) == 1.f);

  p.dst_kind = D::Uint;
  const auto u = scatter(p, src);
  CHECK(u[0] == 51);
  CHECK(u[1] == 102);
  CHECK(u[2] == 0);
  CHECK(u[3] == 1);
}

TEST_CASE("scatterOnCpu decodes halves and integers", "[gfx][scatter][C40]")
{
  std::vector<unsigned char> src(12, 0);
  const qfloat16 h0(-2.5f), h1(0.125f);
  std::memcpy(src.data(), &h0, 2);
  std::memcpy(src.data() + 2, &h1, 2);
  const int32_t neg = -42;
  std::memcpy(src.data() + 4, &neg, 4);
  const uint32_t big = 3000000000u;
  std::memcpy(src.data() + 8, &big, 4);

  GPUBufferScatter::Params half{
      .element_count = 1, .src_components = 2, .dst_components = 3,
      .src_stride_bytes = 4, .dst_stride_floats = 4, .src_format = S::Half};
  const auto h = scatter(half, src);
  CHECK(f(h[0]) == -2.5f);
  CHECK(f(h[1]) == 0.125f);
  CHECK(f(h[2]) == 1.f);   // the last of three
  CHECK(h[3] == 0xDEADBEEF); // std430 padding untouched

  GPUBufferScatter::Params sint{
      .element_count = 1, .src_components = 1, .dst_components = 1,
      .src_stride_bytes = 4, .src_offset_bytes = 4, .src_format = S::Sint32};
  CHECK(f(scatter(sint, src)[0]) == -42.f);
  sint.dst_kind = D::Int;
  CHECK(int32_t(scatter(sint, src)[0]) == -42);

  GPUBufferScatter::Params uint{
      .element_count = 1, .src_components = 1, .dst_components = 1,
      .src_stride_bytes = 4, .src_offset_bytes = 8, .src_format = S::Uint32};
  CHECK(f(scatter(uint, src)[0]) == 3000000000.f);
}

TEST_CASE("scatterOnCpu reads floats as floats and past the end as zeros", "[gfx][scatter][C40]")
{
  std::vector<unsigned char> src(12);
  const float v[3]{1.5f, -3.f, 7.25f};
  std::memcpy(src.data(), v, 12);
  GPUBufferScatter::Params p{
      .element_count = 2, .src_components = 3, .dst_components = 4,
      .src_stride_bytes = 8, .dst_stride_floats = 4};
  const auto d = scatter(p, src);
  CHECK(f(d[0]) == 1.5f);
  CHECK(f(d[1]) == -3.f);
  CHECK(f(d[2]) == 7.25f);
  CHECK(f(d[3]) == 1.f);
  // The second element starts at byte 8: its 2nd and 3rd floats are past the end.
  CHECK(f(d[4]) == 7.25f);
  CHECK(d[5] == 0);
  CHECK(d[6] == 0);

  p.dst_kind = D::Int;
  p.element_count = 1;
  const auto i = scatter(p, src);
  CHECK(int32_t(i[0]) == 1);
  CHECK(int32_t(i[1]) == -3);
  CHECK(int32_t(i[3]) == 1);
}

// A float read as an integer is clamped to the integer's range: a negative
// float read as uint is 0, one past the range its end. The plain conversion
// is undefined behaviour in C++ and undefined in GLSL, so the CPU and GPU
// paths could disagree.
TEST_CASE("scatterOnCpu clamps a float read as an integer", "[gfx][scatter][C40]")
{
  const float values[4]{-2.5f, 1e10f, -1e10f, 7.75f};
  std::vector<unsigned char> src(sizeof(values));
  std::memcpy(src.data(), values, sizeof(values));
  GPUBufferScatter::Params p{
      .element_count = 1, .src_components = 4, .dst_components = 4,
      .src_stride_bytes = 16, .src_format = S::Float32, .dst_kind = D::Uint};
  const auto u = scatter(p, src);
  CHECK(u[0] == 0u);
  CHECK(u[1] == 4294967295u);
  CHECK(u[2] == 0u);
  CHECK(u[3] == 7u);
  p.dst_kind = D::Int;
  const auto i = scatter(p, src);
  CHECK(int32_t(i[0]) == -2);
  CHECK(int32_t(i[1]) == 2147483647);
  CHECK(int32_t(i[2]) == -2147483647 - 1);
  CHECK(int32_t(i[3]) == 7);
}
