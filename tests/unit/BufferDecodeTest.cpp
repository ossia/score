// Buffer view: the bytes of a buffer read as elements of floats, by element
// type, components, stride and offset.

#include <Ui/Monitor/BufferDecode.hpp>
#include <Ui/Monitor/BufferGather.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
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

TEST_CASE("Buffer sampling: a window from the first element", "[ui][buffer_view]")
{
  constexpr int64_t cap = max_readback_bytes;
  // All of them when they fit
  CHECK(sampleElements(100, 0, 1000, 16, cap, false) == Sampling{0, 1, 100, 100, false});
  // From the first asked, as many as asked
  CHECK(sampleElements(100, 30, 50, 16, cap, false) == Sampling{30, 1, 50, 70, false});
  CHECK(sampleElements(100, 80, 50, 16, cap, false) == Sampling{80, 1, 20, 20, false});
  // None past the end
  CHECK(sampleElements(100, 100, 50, 16, cap, false).count == 0);
  CHECK(sampleElements(100, 1000, 50, 16, cap, false).count == 0);
  CHECK(sampleElements(0, 0, 50, 16, cap, false).count == 0);
  // The readback cap wins over the elements asked
  const auto big = sampleElements(int64_t(1) << 30, 0, max_view_elements, 64, cap, false);
  CHECK(big.count == cap / 64);
  CHECK(big.capped);
  // Not capped when the window is small enough anyway
  CHECK(!sampleElements(1000, 0, max_view_elements, 64, cap, false).capped);
}

TEST_CASE("Buffer sampling: an overview steps over the whole window", "[ui][buffer_view]")
{
  constexpr int64_t cap = max_readback_bytes;
  // Fits: read whole, step 1
  CHECK(sampleElements(100, 0, 1000, 16, cap, true) == Sampling{0, 1, 100, 100, false});
  // 1000 elements, 100 at most: every 10th, the last one read is 990
  const auto a = sampleElements(1000, 0, 100, 16, cap, true);
  CHECK(a == Sampling{0, 10, 100, 1000, false});
  // Not a multiple: the step rounds up, the samples cover the window
  const auto b = sampleElements(1001, 0, 100, 16, cap, true);
  CHECK(b.step == 11);
  CHECK(b.count == 91);
  CHECK((b.count - 1) * b.step < b.window);
  CHECK(b.count <= 100);
  // From the first element to the end
  const auto c = sampleElements(1000, 500, 100, 16, cap, true);
  CHECK(c == Sampling{500, 5, 100, 500, false});
  // 2 GiB of vec4: the samples stay within the cap and span the buffer
  const int64_t n = (int64_t(2) << 30) / 16;
  const auto d = sampleElements(n, 0, max_view_elements, 16, cap, true);
  CHECK(d.count * 16 <= cap);
  CHECK(d.first + (d.count - 1) * d.step < n);
  CHECK(d.first + d.count * d.step >= n);
}

TEST_CASE("Buffer sampling: a contiguous read cut to the cap", "[ui][buffer_view]")
{
  // 16 bytes: (16 - 4 - 0 - 3) / 4 + 1
  CHECK(contiguousFit(4, 4, 0, 16) == 3);
  CHECK(contiguousFit(65536, 16, 2, max_readback_bytes) == 256);
  CHECK(contiguousFit(16, 16, 0, 8) == 0);
  // What fits does: a read from a 4-byte boundary to the next after the last
  for(int64_t stride : {4, 12, 16, 100, 65536})
    for(int64_t mis : {0, 1, 3})
    {
      const int64_t fit = contiguousFit(stride, 4, mis, max_readback_bytes);
      const int64_t span = mis + (fit - 1) * stride + 4;
      CHECK((span + 3) / 4 * 4 <= max_readback_bytes);
      CHECK(mis + fit * stride + 4 + 3 > max_readback_bytes);
    }
}

namespace
{
std::vector<unsigned char> pattern(std::size_t n)
{
  std::vector<unsigned char> bytes(n);
  for(std::size_t i = 0; i < n; i++)
    bytes[i] = (unsigned char)((i * 131u + 7u) ^ (i >> 8));
  return bytes;
}

std::vector<uint32_t> words(const std::vector<unsigned char>& bytes, int64_t from, int64_t size)
{
  std::vector<uint32_t> w(std::size_t(size / 4));
  std::memcpy(w.data(), bytes.data() + from, w.size() * 4);
  return w;
}

//! What the gather pass writes, through the shader's arithmetic, compared
//! with the elements copied one by one
void checkGather(const Layout& l, int64_t buffer_bytes, int64_t first, int64_t max, bool overview)
{
  const auto bytes = pattern(std::size_t(buffer_bytes));
  const int64_t available = elementCount(l, gatherReadable(buffer_bytes));
  const auto s
      = sampleElements(available, first, max, elementBytes(l), max_readback_bytes, overview);
  REQUIRE(s.count > 0);
  const auto p = planGather(l, l.offset, buffer_bytes, s, overview, int64_t(1) << 32, 65535);
  REQUIRE(p.valid);
  CHECK(p.bind_offset % gather_bind_alignment == 0);
  CHECK(p.bind_offset + p.bind_size <= gatherReadable(buffer_bytes));
  CHECK(p.bind_size % 4 == 0);
  CHECK(p.readback_size <= max_readback_bytes + gather_header_bytes);
  const auto src = words(bytes, p.bind_offset, p.bind_size);

  std::vector<unsigned char> got(std::size_t(p.params.out_words) * 4);
  for(uint32_t w = 0; w < p.params.out_words; w++)
  {
    const uint32_t v = gatherWord(p.params, src, w);
    std::memcpy(got.data() + w * 4, &v, 4);
  }
  const int64_t e = elementBytes(l);
  bool same = true;
  for(int64_t i = 0; i < s.count && same; i++)
    for(int64_t k = 0; k < e; k++)
    {
      const int64_t at = l.offset + (s.first + i * s.step) * strideOf(l) + k;
      if(got[std::size_t(i * e + k)] != bytes[std::size_t(at)])
      {
        same = false;
        UNSCOPED_INFO("element " << i << " byte " << k);
        break;
      }
    }
  CHECK(same);

  // Decoded as packed elements, the same values as the buffer decoded
  std::vector<float> packed(std::size_t(s.count * components(l)));
  CHECK(decode({l.type, l.components, 0, 0}, got, packed) == s.count);
  std::vector<float> all(std::size_t(available * components(l)));
  decode(l, bytes, all);
  for(int64_t i = 0; i < s.count; i++)
    for(int c = 0; c < components(l); c++)
    {
      const float a = packed[std::size_t(i * components(l) + c)];
      const float b = all[std::size_t((s.first + i * s.step) * components(l) + c)];
      CHECK((a == b || (std::isnan(a) && std::isnan(b))));
    }
}
}

TEST_CASE("Buffer gather: the shader's arithmetic copies the elements read", "[ui][buffer_view]")
{
  SECTION("packed vec4") { checkGather({ElementType::Float, 4}, 4096, 0, 1000, false); }
  SECTION("strided, from an element") { checkGather({ElementType::Float, 2, 48, 8}, 9000, 7, 50, false); }
  SECTION("unaligned offset") { checkGather({ElementType::Float, 3, 0, 3}, 4099, 0, 200, false); }
  SECTION("half x3") { checkGather({ElementType::Half, 3, 0, 2}, 3000, 1, 400, false); }
  SECTION("half x1, odd stride") { checkGather({ElementType::Half, 1, 7, 1}, 3000, 0, 400, false); }
  SECTION("uint8 x3") { checkGather({ElementType::UInt8, 3, 0, 1}, 3001, 0, 999, false); }
  SECTION("uint8 x1 overview") { checkGather({ElementType::UNorm8, 1, 0, 5}, 100000, 0, 333, true); }
  SECTION("vec4 overview from an element") { checkGather({ElementType::Float, 4, 64, 0}, 1 << 20, 100, 500, true); }
  SECTION("bind offset past the start") { checkGather({ElementType::Int, 1, 0, 0}, 8192, 1000, 10, false); }
}

TEST_CASE("Buffer gather: plans", "[ui][buffer_view]")
{
  const Layout vec4{ElementType::Float, 4};
  // The readback holds the elements and nothing between them, however far apart
  const Layout far{ElementType::Float, 4, 65536};
  const int64_t two_gb = int64_t(2) << 30;
  const auto s = sampleElements(elementCount(far, two_gb), 0, 65536, 16, max_readback_bytes, false);
  CHECK(s.count == 32768);
  const auto p = planGather(far, 0, two_gb, s, false, two_gb, 65535);
  REQUIRE(p.valid);
  CHECK(p.readback_size == 32768 * 16);
  CHECK(p.readback_offset == gather_header_bytes);
  CHECK(p.params.sample_stride == 65536);

  // A measure reads the header too, and binds the whole window
  const auto o = sampleElements(two_gb / 16, 0, 65536, 16, max_readback_bytes, true);
  const auto q = planGather(vec4, 0, two_gb, o, true, two_gb, 65535);
  REQUIRE(q.valid);
  CHECK(q.readback_offset == 0);
  CHECK(q.readback_size == gather_header_bytes + o.count * 16);
  CHECK(q.params.reduce_count == two_gb / 16);
  CHECK(q.bind_size == two_gb);
  CHECK(int64_t(q.groups_x) * q.groups_y <= gather_max_groups);
  // Larger than a binding may be: not a gather
  CHECK(!planGather(vec4, 0, two_gb, o, true, two_gb - 1, 65535).valid);
  // Past the buffer: not a gather
  CHECK(!planGather(vec4, 16, 1024, sampleElements(64, 0, 64, 16, max_readback_bytes, false), false, 1 << 20, 65535).valid);
  // In the last bytes of a buffer whose size is not a multiple of 4: out of reach
  const Layout bytes3{ElementType::UInt8, 3};
  CHECK(planGather(bytes3, 0, 1000, sampleElements(333, 0, 333, 3, max_readback_bytes, false), false, 1 << 20, 65535).valid);
  CHECK(elementCount(bytes3, 1002) == 334);
  CHECK(!planGather(bytes3, 0, 1002, sampleElements(334, 0, 334, 3, max_readback_bytes, false), false, 1 << 20, 65535).valid);

  // Groups spread over y when a dimension is too short
  const auto r = planGather(vec4, 0, two_gb, o, true, two_gb, 1000);
  CHECK(r.groups_x == 1000);
  CHECK(r.groups_y == 5);
}

TEST_CASE("Buffer gather: ordered floats and the header", "[ui][buffer_view]")
{
  const float values[]{-std::numeric_limits<float>::max(), -1e10f, -1.f, -1e-30f, 0.f,
                       1e-30f, 0.5f, 1.f, 3e8f, std::numeric_limits<float>::max()};
  for(std::size_t i = 0; i < std::size(values); i++)
  {
    CHECK(unorderedFloat(orderedFloat(values[i])) == values[i]);
    if(i > 0)
      CHECK(orderedFloat(values[i - 1]) < orderedFloat(values[i]));
  }
  uint32_t header[gather_header_words]{};
  std::fill_n(header, 4, 0xffffffffu);
  header[0] = orderedFloat(-2.f);
  header[4] = orderedFloat(5.f);
  header[1] = orderedFloat(0.f);
  header[5] = orderedFloat(0.f);
  header[8] = 3;
  const auto r = readGatherHeader(header, 3);
  CHECK(r.lo[0] == -2.f);
  CHECK(r.hi[0] == 5.f);
  CHECK(r.lo[1] == 0.f);
  CHECK(r.hi[1] == 0.f);
  // Nothing finite in the third component
  CHECK(r.lo[2] > r.hi[2]);
  CHECK(r.non_finite == 3);
}
