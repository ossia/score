// Instancer::prepareBake decides how many instances the bake compute shader
// reads. The shader indexes its source storage buffers without bounds checks,
// so the count must be clamped to what every source actually holds, and the
// baked target sizes must not overflow 32 bits.
//
// Runs on QRhi's Null backend: the decision is on the CPU, no shader runs.
#include <Threedim/Instancer.hpp>

#include <QMatrix4x4>
#include <QtGlobal>

#include <QtGui/private/qrhi_p.h>
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#include <rhi/qrhi_platform.h>
#else
#include <QtGui/private/qrhinull_p.h>
#endif

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

namespace
{
struct Bench
{
  std::unique_ptr<QRhi> rhi;
  Threedim::Instancer node;

  Bench()
  {
    QRhiNullInitParams params;
    rhi.reset(QRhi::create(QRhi::Null, &params));
    REQUIRE(rhi);
    node.m_rhi = rhi.get();
    node.m_bakePipeline = rhi->newComputePipeline();
    node.m_bakeDummy = storage(64);
  }

  ~Bench()
  {
    delete node.m_bakePipeline;
    delete node.m_bakedTransforms;
    delete node.m_bakedColors;
    delete node.m_bakedCustom;
    node.m_bakePipeline = nullptr;
    node.m_bakeDummy = nullptr;
    node.m_bakedTransforms = nullptr;
    node.m_bakedColors = nullptr;
    node.m_bakedCustom = nullptr;
    for(auto* b : owned)
      delete b;
  }

  QRhiBuffer* storage(quint32 size, bool storage_usage = true)
  {
    auto* b = rhi->newBuffer(
        QRhiBuffer::Static,
        storage_usage ? QRhiBuffer::StorageBuffer : QRhiBuffer::VertexBuffer, size);
    REQUIRE(b->create());
    owned.push_back(b);
    return b;
  }

  std::vector<QRhiBuffer*> owned;
};

using Src = Threedim::Instancer::BakeSource;
constexpr int N = Threedim::Instancer::BakeSourceCount;
}

TEST_CASE("the bake count is clamped to the transforms a buffer holds", "[threedim][instancer][bake]")
{
  Bench b;
  Src sources[N]{};
  // Ten mat4 at a 64-byte stride.
  sources[Threedim::Instancer::BakeTransforms] = {b.storage(640), 0, 64, 4};
  uint32_t count = 100;
  REQUIRE(b.node.prepareBake(sources, 2, count, true, QMatrix4x4{}));
  CHECK(count == 10);
  CHECK(b.node.m_bake.count == 10);
}

TEST_CASE("a packed float3 colour stream bounds the count by its last element", "[threedim][instancer][bake]")
{
  Bench b;
  Src sources[N]{};
  // Five float3 colours, 12 bytes each, tightly packed after a 16-byte offset.
  sources[Threedim::Instancer::BakeColors] = {b.storage(16 + 60), 16, 12, 3};
  uint32_t count = 1000;
  REQUIRE(b.node.prepareBake(sources, 0, count, false, QMatrix4x4{}));
  CHECK(count == 5);
}

TEST_CASE("an interleaved stream bounds the count by its stride", "[threedim][instancer][bake]")
{
  Bench b;
  Src sources[N]{};
  // A rotation quaternion every 48 bytes: 4 whole elements plus a last one
  // whose 16 bytes still fit, in 4 * 48 + 16 bytes.
  sources[Threedim::Instancer::BakeRotations] = {b.storage(4 * 48 + 16), 0, 48, 4};
  uint32_t count = 50;
  REQUIRE(b.node.prepareBake(sources, 0, count, true, QMatrix4x4{}));
  CHECK(count == 5);
}

TEST_CASE("sources the shader does not read do not bound the count", "[threedim][instancer][bake]")
{
  Bench b;
  Src sources[N]{};
  sources[Threedim::Instancer::BakeTransforms] = {b.storage(640), 0, 64, 4};
  // Custom is only read for full matrices, rotations and scales only for
  // translation transforms.
  sources[Threedim::Instancer::BakeCustom] = {b.storage(32), 0, 16, 4};
  sources[Threedim::Instancer::BakeRotations] = {b.storage(32), 0, 16, 4};
  sources[Threedim::Instancer::BakeScales] = {b.storage(32), 0, 16, 3};
  uint32_t count = 10;
  REQUIRE(b.node.prepareBake(sources, 2, count, false, QMatrix4x4{}));
  CHECK(count == 10);

  count = 10;
  REQUIRE(b.node.prepareBake(sources, 2, count, true, QMatrix4x4{}));
  CHECK(count == 2);
}

TEST_CASE("a source too small for one element is not baked", "[threedim][instancer][bake]")
{
  Bench b;
  Src sources[N]{};
  // A TRS transform reads 40 bytes; the buffer past the offset holds 32.
  sources[Threedim::Instancer::BakeTransforms] = {b.storage(64), 32, 40, 4};
  uint32_t count = 1;
  CHECK(!b.node.prepareBake(sources, 1, count, true, QMatrix4x4{}));
  CHECK(!b.node.m_baking);
}

TEST_CASE("a source that is not a storage buffer is not baked", "[threedim][instancer][bake]")
{
  Bench b;
  Src sources[N]{};
  sources[Threedim::Instancer::BakeColors] = {b.storage(1024, false), 0, 16, 4};
  uint32_t count = 4;
  CHECK(!b.node.prepareBake(sources, 0, count, true, QMatrix4x4{}));
}

TEST_CASE("a count whose baked matrices exceed 4 GiB is not baked", "[threedim][instancer][bake]")
{
  Bench b;
  const Src sources[N]{};
  // 100M mat4 = 6.4 GB: in 32 bits that wraps to a 2 GB buffer the shader
  // would then write 6.4 GB into.
  uint32_t count = 100'000'000;
  CHECK(!b.node.prepareBake(sources, 0, count, true, QMatrix4x4{}));
  CHECK(!b.node.m_bakedTransforms);
}
