// The draw-count clamp (IndirectCountClamp, deddae7395) as a unit, on the Null
// QRhi backend: no GPU, no display.
//
// - Its parameters live in a per-frame uniform buffer: a second run in one
//   frame overwrote the first's before either dispatch executed. That was
//   only safe because its one caller runs it once per frame; run() refuses a
//   second run in the same frame now.
// - Growing the command buffer rebuilt the bindings and deleted the old ones,
//   which the pipeline kept pointing to. It is given the new ones.
#include <Gfx/Graph/IndirectCountClamp.hpp>
#include <Gfx/Graph/RenderState.hpp>

#include <catch2/catch_test_macros.hpp>

#include <QtGui/private/qrhi_p.h>
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#include <rhi/qrhi_platform.h>
#else
#include <QtGui/private/qrhinull_p.h>
#endif

#include <cstring>
#include <memory>

using namespace score::gfx;

namespace
{
struct Fixture
{
  std::unique_ptr<QRhi> rhi;
  RenderState state;
  std::unique_ptr<QRhiBuffer> commands, count;
  Fixture(quint32 slots)
  {
    QRhiNullInitParams params;
    rhi.reset(QRhi::create(QRhi::Null, &params));
    state.rhi = rhi.get();
    state.api = GraphicsApi::Null;
    QRhiBuffer::UsageFlags usage = QRhiBuffer::StorageBuffer;
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    usage |= QRhiBuffer::IndirectBuffer;
#endif
    commands.reset(rhi->newBuffer(QRhiBuffer::Static, usage, slots * 20));
    count.reset(rhi->newBuffer(QRhiBuffer::Static, QRhiBuffer::StorageBuffer, 4));
    commands->create();
    count->create();
  }
  QRhiBuffer* run(IndirectCountClamp& clamp, quint32 slots, int64_t frame)
  {
    QRhiCommandBuffer* cb{};
    if(rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
      return nullptr;
    QRhiResourceUpdateBatch* res{};
    auto* out = clamp.run(
        state, *cb, res,
        {.commands = commands.get(), .stride = 20, .slots = slots, .count = count.get(),
         .frame = frame});
    if(res)
      res->release();
    rhi->endOffscreenFrame();
    return out;
  }
};
}

TEST_CASE("the draw-count clamp runs once per frame", "[gfx][indirect][clamp]")
{
  Fixture f{16};
  REQUIRE(f.rhi);
  IndirectCountClamp clamp;
  REQUIRE(f.run(clamp, 16, 1));
  CHECK(clamp.refusal() == nullptr);
  CHECK(f.run(clamp, 16, 1) == nullptr);
  REQUIRE(clamp.refusal());
  CHECK(std::strstr(clamp.refusal(), "frame") != nullptr);
  CHECK(f.run(clamp, 16, 2));
  clamp.release();
}

TEST_CASE("the draw-count clamp's pipeline uses the bindings it dispatches with", "[gfx][indirect][clamp]")
{
  Fixture f{64};
  REQUIRE(f.rhi);
  IndirectCountClamp clamp;
  REQUIRE(f.run(clamp, 4, 1));
  CHECK(clamp.bindingsId() != 0);
  CHECK(clamp.pipelineBindingsId() == clamp.bindingsId());
  // More slots: the clamped buffer grows and the bindings are rebuilt.
  REQUIRE(f.run(clamp, 64, 2));
  CHECK(clamp.pipelineBindingsId() == clamp.bindingsId());
  clamp.release();
}
