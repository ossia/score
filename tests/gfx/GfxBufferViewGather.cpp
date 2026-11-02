// The buffer view's gather pass: what it reads back must be the elements the
// view asked for, packed one after the other, whatever the stride, the offset
// and the size of the components; in an overview, the extent of every element
// of the window and the count of values not finite must be those the CPU
// finds decoding the whole buffer.
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/RenderState.hpp>

#include <Ui/Monitor/BufferGatherPass.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

using namespace score::test::gfx;
using namespace Ui::Monitor::Buffer;

namespace
{
struct Case
{
  const char* name;
  Layout layout;
  int64_t buffer_bytes;
  int64_t first;
  int64_t max;
  bool overview;
};

//! Bytes where every element holds finite values but two, and the bytes
//! between the elements hold anything
std::vector<unsigned char> contents(const Layout& l, int64_t size)
{
  std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
  for(std::size_t i = 0; i < bytes.size(); i++)
    bytes[i] = (unsigned char)((i * 131u + 7u) ^ (i >> 8));
  const int64_t n = elementCount(l, size);
  const int cb = componentBytes(l.type);
  for(int64_t i = 0; i < n; i++)
    for(int c = 0; c < components(l); c++)
    {
      unsigned char* p = bytes.data() + l.offset + i * strideOf(l) + c * cb;
      switch(l.type)
      {
        case ElementType::Float: {
          float v = float(std::sin(double(i) * 0.37 + c) * 100. + c);
          if(i == 5 && c == 0)
            v = std::numeric_limits<float>::quiet_NaN();
          if(i == n - 1)
            v = std::numeric_limits<float>::infinity();
          std::memcpy(p, &v, 4);
          break;
        }
        case ElementType::Half: {
          // Normal and finite: no denormal a GPU could flush
          uint16_t v = uint16_t(0x3c00 + (i * 7 + c) % 0x3000);
          if(i % 2)
            v |= 0x8000;
          if(i == 3 && c == 0)
            v = 0x7c00;
          std::memcpy(p, &v, 2);
          break;
        }
        case ElementType::Int:
        case ElementType::UInt: {
          const uint32_t v = uint32_t(i * 2654435761u + c) >> 8;
          std::memcpy(p, &v, 4);
          break;
        }
        default:
          break;
      }
    }
  return bytes;
}

struct Outcome
{
  std::string skip;
  std::string error;
  QByteArray data;
  Sampling sampling;
  GatherPlan plan;
};

Outcome run(score::gfx::GraphicsApi backend, const Case& k, const std::vector<unsigned char>& bytes)
{
  Outcome o;
  const Layout& l = k.layout;
  const int64_t available = elementCount(l, gatherReadable(k.buffer_bytes));
  o.sampling = sampleElements(
      available, k.first, k.max, elementBytes(l),
      max_readback_bytes - (k.overview ? gather_header_bytes : 0), k.overview);

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    std::string probed;
    if(!probe_api(backend, probed))
    {
      o.skip = probed;
      return;
    }
    auto st = score::gfx::createRenderState(backend, QSize{16, 16}, nullptr);
    if(!st || !st->rhi)
    {
      o.skip = "no rhi";
      return;
    }
    QRhi& rhi = *st->rhi;
    if(!rhi.isFeatureSupported(QRhi::Compute))
    {
      o.skip = "no compute shaders on this backend";
      st->destroy();
      return;
    }
    o.plan = planGather(
        l, l.offset, k.buffer_bytes, o.sampling, k.overview, st->caps.maxStorageBufferSize,
        rhi.resourceLimit(QRhi::MaxThreadGroupsPerDimension));
    auto* src = rhi.newBuffer(
        QRhiBuffer::Static, QRhiBuffer::StorageBuffer, quint32(k.buffer_bytes));
    src->create();
    {
      GatherPass pass;
      QRhiBufferReadbackResult rb;
      QRhiCommandBuffer* cb{};
      if(!o.plan.valid)
        o.error = "plan not valid";
      else if(pass.check(rhi, *src) != Fallback::None)
        o.error = "the pass refuses the buffer";
      else if(rhi.beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
        o.error = "beginOffscreenFrame failed";
      else
      {
        QRhiResourceUpdateBatch* res = rhi.nextResourceUpdateBatch();
        res->uploadStaticBuffer(src, 0, quint32(bytes.size()), bytes.data());
        if(!pass.record(*st, *cb, res, *src, o.plan, rb))
        {
          o.error = "record failed";
          res->release();
        }
        rhi.endOffscreenFrame();
        rhi.finish();
        o.data = rb.data;
      }
      pass.release();
    }
    delete src;
    st->destroy();
  });
  return o;
}

void check(const Case& k)
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend), k.name);
  const Layout& l = k.layout;
  const auto bytes = contents(l, k.buffer_bytes);
  const auto o = run(backend, k, bytes);
  if(!o.skip.empty())
    SKIP(o.skip);
  INFO("error=" << o.error);
  REQUIRE(o.error.empty());
  const auto& s = o.sampling;
  REQUIRE(s.count > 0);
  REQUIRE(o.data.size() == o.plan.readback_size);
  CHECK(o.data.size() <= max_readback_bytes);

  const int64_t e = elementBytes(l);
  const int64_t head = k.overview ? gather_header_bytes : 0;
  const auto* got = reinterpret_cast<const unsigned char*>(o.data.constData()) + head;
  int64_t bad = 0, firstBad = -1;
  for(int64_t i = 0; i < s.count; i++)
    for(int64_t b = 0; b < e; b++)
      if(got[i * e + b]
         != bytes[std::size_t(l.offset + (s.first + i * s.step) * strideOf(l) + b)])
      {
        if(bad++ == 0)
          firstBad = i;
      }
  INFO("first mismatching element: " << firstBad);
  CHECK(bad == 0);

  if(!k.overview)
    return;
  // The extents of every element from the first, decoded on the CPU
  const int n = components(l);
  const int64_t available = elementCount(l, gatherReadable(k.buffer_bytes));
  std::vector<float> all(std::size_t(available * n));
  decode(l, bytes, all);
  std::array<float, 4> lo, hi;
  lo.fill(std::numeric_limits<float>::max());
  hi.fill(std::numeric_limits<float>::lowest());
  int64_t nonFinite = 0;
  for(int64_t i = s.first; i < available; i++)
    for(int c = 0; c < n; c++)
    {
      const float v = all[std::size_t(i * n + c)];
      if(!std::isfinite(v))
      {
        nonFinite++;
        continue;
      }
      lo[c] = std::min(lo[c], v);
      hi[c] = std::max(hi[c], v);
    }
  uint32_t header[gather_header_words];
  std::memcpy(header, o.data.constData(), sizeof(header));
  const auto r = readGatherHeader(header, n);
  for(int c = 0; c < n; c++)
  {
    CAPTURE(c);
    CHECK(r.lo[c] == lo[c]);
    CHECK(r.hi[c] == hi[c]);
  }
  CHECK(r.non_finite == nonFinite);
}
}

TEST_CASE("Buffer view gather: packed vec4", "[gfx][compute][buffer_view]")
{
  check({"packed", {ElementType::Float, 4}, 16 * 5000, 0, 5000, false});
}

TEST_CASE("Buffer view gather: a large stride reads no gap", "[gfx][compute][buffer_view]")
{
  check({"strided", {ElementType::Float, 3, 4096, 16}, 4096 * 3000, 10, 2000, false});
}

TEST_CASE("Buffer view gather: an unaligned offset", "[gfx][compute][buffer_view]")
{
  check({"unaligned", {ElementType::Float, 2, 0, 3}, 4099, 0, 500, false});
  check({"unaligned int", {ElementType::Int, 1, 9, 1}, 4000, 3, 400, false});
}

TEST_CASE("Buffer view gather: half and uint8 components", "[gfx][compute][buffer_view]")
{
  check({"half x3", {ElementType::Half, 3, 0, 2}, 6002, 0, 1000, false});
  check({"uint8 x3", {ElementType::UInt8, 3, 0, 1}, 3001, 1, 999, false});
  check({"unorm8 x1", {ElementType::UNorm8, 1, 3, 0}, 1000, 0, 333, false});
}

TEST_CASE(
    "Buffer view gather: an overview samples the window and measures all of it",
    "[gfx][compute][buffer_view]")
{
  // Many workgroups, each invocation over several elements
  check({"vec4", {ElementType::Float, 4}, 16 << 20, 0, 10000, true});
  check({"vec4 from an element", {ElementType::Float, 4, 32}, 1 << 20, 777, 1000, true});
  check({"half x3", {ElementType::Half, 3, 0, 2}, 600002, 0, 1000, true});
  check({"uint8 x3", {ElementType::UInt8, 3, 0, 1}, 300001, 0, 999, true});
  check({"uint", {ElementType::UInt, 1}, 400000, 0, 1000, true});
}
