// Buffer view: the bytes of a buffer read as elements of floats, by element
// type, components, stride and offset.

#include <Ui/Monitor/BufferDecode.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

using namespace Ui::Monitor::Buffer;

namespace
{
template <typename T>
void put(std::vector<unsigned char>& bytes, std::size_t at, T v)
{
  if(bytes.size() < at + sizeof(T))
    bytes.resize(at + sizeof(T));
  std::memcpy(bytes.data() + at, &v, sizeof(T));
}

std::vector<float> decodeAll(const Layout& l, const std::vector<unsigned char>& bytes)
{
  std::vector<float> out(std::size_t(elementCount(l, int64_t(bytes.size())) * components(l)));
  const auto n = decode(l, bytes, out);
  out.resize(std::size_t(n * components(l)));
  return out;
}
}

TEST_CASE("Buffer decode: element sizes and counts", "[ui][buffer_view]")
{
  CHECK(elementBytes({ElementType::Float, 4}) == 16);
  CHECK(elementBytes({ElementType::Half, 3}) == 6);
  CHECK(elementBytes({ElementType::UInt8, 4}) == 4);
  // Out of range components are clamped to 1 to 4
  CHECK(elementBytes({ElementType::Int, 0}) == 4);
  CHECK(elementBytes({ElementType::Int, 9}) == 16);

  CHECK(strideOf({ElementType::Float, 3}) == 12);
  CHECK(strideOf({ElementType::Float, 3, 16}) == 16);

  // 384 bytes of vec4: 24 elements
  CHECK(elementCount({ElementType::Float, 4}, 384) == 24);
  // A partial element at the end is not one
  CHECK(elementCount({ElementType::Float, 4}, 380) == 23);
  // The last element only needs its own bytes, not a whole stride
  CHECK(elementCount({ElementType::Float, 3, 16}, 16 * 3 + 12) == 4);
  CHECK(elementCount({ElementType::Float, 1, 0, 8}, 16) == 2);
  CHECK(elementCount({ElementType::Float, 1, 0, 16}, 16) == 0);
  CHECK(elementCount({ElementType::Float, 1, 0, 20}, 16) == 0);
  CHECK(elementCount({ElementType::Float, 1}, 0) == 0);
}

TEST_CASE("Buffer decode: types", "[ui][buffer_view]")
{
  std::vector<unsigned char> bytes;
  put(bytes, 0, 1.5f);
  put(bytes, 4, -2.25f);
  CHECK(decodeAll({ElementType::Float, 1}, bytes) == std::vector<float>{1.5f, -2.25f});

  bytes.clear();
  put(bytes, 0, int32_t(-7));
  put(bytes, 4, int32_t(123456));
  CHECK(decodeAll({ElementType::Int, 2}, bytes) == std::vector<float>{-7.f, 123456.f});

  bytes.clear();
  put(bytes, 0, uint32_t(4000000000u));
  CHECK(decodeAll({ElementType::UInt, 1}, bytes) == std::vector<float>{4e9f});

  bytes = {0, 51, 255, 128};
  CHECK(decodeAll({ElementType::UInt8, 4}, bytes) == std::vector<float>{0.f, 51.f, 255.f, 128.f});
  const auto n = decodeAll({ElementType::UNorm8, 4}, bytes);
  REQUIRE(n.size() == 4);
  CHECK(n[0] == 0.f);
  CHECK(n[1] == 0.2f);
  CHECK(n[2] == 1.f);
}

TEST_CASE("Buffer decode: half floats", "[ui][buffer_view]")
{
  CHECK(halfToFloat(0x0000) == 0.f);
  CHECK(std::signbit(halfToFloat(0x8000)));
  CHECK(halfToFloat(0x3c00) == 1.f);
  CHECK(halfToFloat(0xc000) == -2.f);
  CHECK(halfToFloat(0x3555) == 0.333251953125f);
  CHECK(halfToFloat(0x7bff) == 65504.f);
  // Smallest subnormal and normal
  CHECK(halfToFloat(0x0001) == std::ldexp(1.f, -24));
  CHECK(halfToFloat(0x0400) == std::ldexp(1.f, -14));
  CHECK(halfToFloat(0x7c00) == std::numeric_limits<float>::infinity());
  CHECK(halfToFloat(0xfc00) == -std::numeric_limits<float>::infinity());
  CHECK(std::isnan(halfToFloat(0x7e00)));

  std::vector<unsigned char> bytes;
  put(bytes, 0, uint16_t(0x3c00));
  put(bytes, 2, uint16_t(0xc000));
  CHECK(decodeAll({ElementType::Half, 2}, bytes) == std::vector<float>{1.f, -2.f});
}

TEST_CASE("Buffer decode: stride and offset", "[ui][buffer_view]")
{
  // A struct { vec3 position; float pad; vec2 uv; } of 24 bytes, uv at 16
  std::vector<unsigned char> bytes;
  for(int i = 0; i < 3; i++)
  {
    const std::size_t base = std::size_t(i) * 24;
    put(bytes, base + 0, float(i));
    put(bytes, base + 4, float(i * 10));
    put(bytes, base + 8, float(i * 100));
    put(bytes, base + 12, -1.f);
    put(bytes, base + 16, float(i) + 0.5f);
    put(bytes, base + 20, float(i) + 0.25f);
  }
  CHECK(
      decodeAll({ElementType::Float, 3, 24, 0}, bytes)
      == std::vector<float>{0.f, 0.f, 0.f, 1.f, 10.f, 100.f, 2.f, 20.f, 200.f});
  CHECK(
      decodeAll({ElementType::Float, 2, 24, 16}, bytes)
      == std::vector<float>{0.5f, 0.25f, 1.5f, 1.25f, 2.5f, 2.25f});

  // An offset that is not aligned: the bytes need no alignment
  bytes.assign(3, 0);
  put(bytes, 3, 7.f);
  put(bytes, 7, 8.f);
  CHECK(decodeAll({ElementType::Float, 1, 0, 3}, bytes) == std::vector<float>{7.f, 8.f});
}

TEST_CASE("Buffer decode: bounded by the output", "[ui][buffer_view]")
{
  std::vector<unsigned char> bytes;
  for(int i = 0; i < 100; i++)
    put(bytes, std::size_t(i) * 4, float(i));
  std::vector<float> out(10, -1.f);
  const Layout l{ElementType::Float, 3};
  // Three whole elements fit in ten floats
  CHECK(decode(l, bytes, out) == 3);
  CHECK(out[8] == 8.f);
  CHECK(out[9] == -1.f);

  std::vector<float> none;
  CHECK(decode(l, bytes, none) == 0);
  CHECK(decode(l, {}, out) == 0);
}
