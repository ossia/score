// The byte range a borrowed buffer is bound with, and whether it can back a
// shader block at all (shader board U42). Pure decisions over QRhiBuffer
// usage and size, so the Null QRhi backend is enough: no GPU, no display.
#include <Gfx/Graph/SSBO.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>

#include <QtGlobal>

#include <QtGui/private/qrhi_p.h>
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#include <rhi/qrhi_platform.h>
#else
#include <QtGui/private/qrhinull_p.h>
#endif

#include <memory>

using namespace score::gfx;

TEST_CASE("a published range binds when its offset is aligned", "[gfx][binding][U42]")
{
  QRhiNullInitParams params;
  std::unique_ptr<QRhi> rhi{QRhi::create(QRhi::Null, &params)};
  REQUIRE(rhi);
  std::unique_ptr<QRhiBuffer> buf{
      rhi->newBuffer(QRhiBuffer::Static, QRhiBuffer::StorageBuffer, 4096)};
  REQUIRE(buf->create());

  // Offset 0: the whole buffer, the binding's default.
  auto r = bindableRange(buf.get(), 0, 384, "test");
  CHECK(r.offset == 0);
  CHECK(r.size == 0);
  // An aligned offset binds the published bytes.
  r = bindableRange(buf.get(), 256, 384, "test");
  CHECK(r.offset == 256);
  CHECK(r.size == 384);
  // A size running past the end is clamped to it; size 0 = to the end.
  r = bindableRange(buf.get(), 3840, 1024, "test");
  CHECK(r.offset == 3840);
  CHECK(r.size == 256);
  r = bindableRange(buf.get(), 1024, 0, "test");
  CHECK(r.size == 3072);
  // Unaligned, or past the end: no range can be bound, the whole buffer is.
  r = bindableRange(buf.get(), 16, 64, "test");
  CHECK(r.offset == 0);
  CHECK(r.size == 0);
  r = bindableRange(buf.get(), 8192, 64, "test");
  CHECK(r.offset == 0);
  CHECK(r.size == 0);
}

TEST_CASE("a buffer backs a block of its own kind and size", "[gfx][binding][U42]")
{
  QRhiNullInitParams params;
  std::unique_ptr<QRhi> rhi{QRhi::create(QRhi::Null, &params)};
  REQUIRE(rhi);
  std::unique_ptr<QRhiBuffer> ubo{
      rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 256)};
  std::unique_ptr<QRhiBuffer> ssbo{
      rhi->newBuffer(QRhiBuffer::Static, QRhiBuffer::StorageBuffer, 256)};
  REQUIRE(ubo->create());
  REQUIRE(ssbo->create());

  CHECK(blockRefusal(ubo.get(), true, 0, 0, 240).isEmpty());
  CHECK(blockRefusal(ssbo.get(), false, 0, 0, 0).isEmpty());
  // Wrong usage, either way.
  CHECK(blockRefusal(ssbo.get(), true, 0, 0, 240).contains("UniformBuffer usage"));
  CHECK(blockRefusal(ubo.get(), false, 0, 0, 0).contains("StorageBuffer usage"));
  // Too small: the buffer, the published range, or what is left past the offset.
  CHECK(blockRefusal(ubo.get(), true, 0, 0, 512).contains("smaller than"));
  CHECK(blockRefusal(ubo.get(), true, 0, 128, 240).contains("smaller than"));
  CHECK(blockRefusal(ubo.get(), true, 64, 0, 240).contains("smaller than"));
  // No declared size: no extent check.
  CHECK(blockRefusal(ubo.get(), true, 0, 16, 0).isEmpty());
}

// The extent a uniform block needs: a trailing array counts as one element,
// since its producer publishes as many as it has (the parser-injected camera
// array is declared [16]); anything else is needed whole.
TEST_CASE("a uniform block needs its fixed prefix and one trailing element", "[gfx][binding][U42]")
{
  isf::descriptor d;
  using F = isf::uniform_input::layout_field;
  const std::vector<F> trailing{{"count", "vec4"}, {"data", "mat4[16]"}};
  CHECK(score::gfx::calculateUniformBlockSize(trailing, 0, d) == 16 + 16 * 64);
  CHECK(score::gfx::minimumUniformBlockSize(trailing, d) == 16 + 64);
  const std::vector<F> inner{{"first", "vec4"}, {"m", "mat4[3]"}, {"last", "vec4"}};
  CHECK(score::gfx::minimumUniformBlockSize(inner, d)
        == score::gfx::calculateUniformBlockSize(inner, 0, d));
  CHECK(score::gfx::minimumUniformBlockSize(inner, d) == 16 + 192 + 16);
}
