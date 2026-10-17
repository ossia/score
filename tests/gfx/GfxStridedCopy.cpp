// GPUStridedCopy gathers strided elements between two storage buffers in one
// compute dispatch; the scene preprocessor uses it in place of one copy region
// per element. Its result must be exactly what those regions would write: the
// copied words moved, every other destination word (the padding of a wider
// destination slot, the bytes before the first element) left alone.
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/GPUStridedCopy.hpp>
#include <Gfx/Graph/RenderState.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cstring>
#include <string>
#include <vector>

using namespace score::test::gfx;

namespace
{
constexpr uint32_t kSentinel = 0xDEADBEEFu;

struct Layout
{
  uint32_t count;
  uint32_t element_words;
  uint32_t src_offset_words;
  uint32_t src_stride_words;
  uint32_t dst_offset_words;
  uint32_t dst_stride_words;
};

struct Outcome
{
  std::string skip;
  std::string error;
  std::vector<uint32_t> got;
  std::vector<uint32_t> expected;
};

Outcome run(score::gfx::GraphicsApi backend, const Layout& l)
{
  Outcome o;
  const uint32_t srcWords = l.src_offset_words + l.count * l.src_stride_words;
  const uint32_t dstWords = l.dst_offset_words + l.count * l.dst_stride_words;

  std::vector<uint32_t> src(srcWords);
  for(uint32_t i = 0; i < srcWords; i++)
    src[i] = 0x10000u + i;
  o.expected.assign(dstWords, kSentinel);
  for(uint32_t e = 0; e < l.count; e++)
    for(uint32_t w = 0; w < l.element_words; w++)
      o.expected[l.dst_offset_words + e * l.dst_stride_words + w]
          = src[l.src_offset_words + e * l.src_stride_words + w];

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

    auto* srcBuf = rhi.newBuffer(
        QRhiBuffer::Static, QRhiBuffer::StorageBuffer, srcWords * 4);
    auto* dstBuf = rhi.newBuffer(
        QRhiBuffer::Static, QRhiBuffer::StorageBuffer, dstWords * 4);
    srcBuf->create();
    dstBuf->create();

    score::gfx::GPUStridedCopy copy;
    const score::gfx::GPUStridedCopy::Params p{
        srcBuf,
        dstBuf,
        l.count,
        l.element_words * 4,
        l.src_offset_words * 4,
        l.src_stride_words * 4,
        l.dst_offset_words * 4,
        l.dst_stride_words * 4};
    if(!score::gfx::GPUStridedCopy::eligible(p))
      o.error = "layout not eligible";
    else if(!copy.init(*st))
      o.error = "GPUStridedCopy::init failed";
    else
    {
      auto op = copy.prepare(rhi, p);
      std::vector<uint32_t> dstInit(dstWords, kSentinel);
      QRhiBufferReadbackResult rb;
      QRhiCommandBuffer* cb{};
      if(rhi.beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
        o.error = "beginOffscreenFrame failed";
      else
      {
        auto* res = rhi.nextResourceUpdateBatch();
        res->uploadStaticBuffer(srcBuf, src.data());
        res->uploadStaticBuffer(dstBuf, dstInit.data());
        copy.updateParams(*res, op, p);
        cb->beginComputePass(res);
        copy.dispatch(*cb, op, p);
        auto* rbBatch = rhi.nextResourceUpdateBatch();
        rbBatch->readBackBuffer(dstBuf, 0, dstWords * 4, &rb);
        cb->endComputePass(rbBatch);
        rhi.endOffscreenFrame();
        rhi.finish();
        o.got.resize(rb.data.size() / 4);
        std::memcpy(o.got.data(), rb.data.constData(), o.got.size() * 4);
      }
      score::gfx::GPUStridedCopy::releaseOp(op);
    }
    copy.release();
    delete srcBuf;
    delete dstBuf;
    st->destroy();
  });
  return o;
}

void check(const Layout& l)
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend), l.count, l.element_words, l.src_stride_words,
          l.dst_stride_words);
  const auto o = run(backend, l);
  if(!o.skip.empty())
    SKIP(o.skip);
  INFO("error=" << o.error);
  REQUIRE(o.error.empty());
  REQUIRE(o.got.size() == o.expected.size());
  std::size_t bad = 0, first = o.got.size();
  for(std::size_t i = 0; i < o.got.size(); i++)
    if(o.got[i] != o.expected[i] && bad++ == 0)
      first = i;
  INFO("first mismatching word: " << first);
  CHECK(bad == 0);
}
}

TEST_CASE(
    "GPUStridedCopy writes what per-element copy regions would",
    "[gfx][compute][scene]")
{
  // A vec3 per element, packed tightly behind other data in the source and
  // into 16-byte slots in the destination (the per-instance array's shape):
  // the slot's 4th word must survive.
  check({37, 3, 2, 5, 1, 4});
}

TEST_CASE(
    "GPUStridedCopy covers element counts spanning many workgroups",
    "[gfx][compute][scene]")
{
  // 3 words x 1000 elements: more invocations than one 256-wide workgroup.
  check({1000, 3, 0, 3, 0, 8});
}
