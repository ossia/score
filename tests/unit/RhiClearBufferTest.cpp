// RhiClearBuffer's update-batch path clears a buffer with uploads of a zero
// block. The block was as large as the largest clear ever asked for, kept per
// thread forever: clearing a 4 GiB storage buffer (resizeCountedStorage, U44)
// held 4 GiB of host zeros for the rest of the process. The uploads are
// chunked from a bounded block. Null QRhi backend.
#include <Gfx/Graph/RhiClearBuffer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <QtGui/private/qrhi_p.h>
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#include <rhi/qrhi_platform.h>
#else
#include <QtGui/private/qrhinull_p.h>
#endif

#include <memory>

TEST_CASE("clearing a large buffer through a batch holds a bounded zero block", "[gfx][clear]")
{
  QRhiNullInitParams params;
  std::unique_ptr<QRhi> rhi{QRhi::create(QRhi::Null, &params)};
  REQUIRE(rhi);
  constexpr quint32 size = 64u * 1024u * 1024u;
  std::unique_ptr<QRhiBuffer> buf{
      rhi->newBuffer(QRhiBuffer::Static, QRhiBuffer::StorageBuffer, size)};
  REQUIRE(buf->create());
  auto* batch = rhi->nextResourceUpdateBatch();
  REQUIRE(batch);
  score::gfx::RhiClearBuffer::clearBuffer(*rhi, *batch, buf.get(), 0, size);
  batch->release();
  INFO("pool bytes: " << score::gfx::RhiClearBuffer::poolBytes());
  CHECK(score::gfx::RhiClearBuffer::poolBytes() <= 2u * 1024u * 1024u);
}
