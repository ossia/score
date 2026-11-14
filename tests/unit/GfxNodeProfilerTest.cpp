#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeProfiler.hpp>

#include <QGuiApplication>

#include <catch2/catch_test_macros.hpp>
#include <rhi/qrhi.h>

namespace
{
struct TestNode final : score::gfx::Node
{
  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const noexcept override
  {
    return nullptr;
  }
};
}

TEST_CASE("Without GPU timestamps a frame is submitted with its CPU times", "[gfx][profiler]")
{
  int argc = 0;
  QGuiApplication app{argc, nullptr};
  QRhiNullInitParams params;
  std::unique_ptr<QRhi> rhi{QRhi::create(QRhi::Null, &params)};
  REQUIRE(rhi);

  TestNode a, b;
  a.nodeId = 4;
  b.nodeId = 7;

  score::gfx::NodeProfiler out;
  score::gfx::RenderListProfiler prof{*rhi, &out};
  REQUIRE(!prof.hasGpuTime());

  QRhiCommandBuffer* cb{};
  REQUIRE(rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess);

  prof.beginFrame(*cb, out, 12);
  prof.addCpu(a, 100);
  prof.addCpu(b, 50);
  prof.addCpu(a, 20);
  prof.setTargetBytes(b, 4096);
  prof.stamp(*cb, a);
  prof.endFrame(out, 1000);
  REQUIRE(out.take().empty());

  out.setEnabled(true);
  prof.beginFrame(*cb, out, 13);
  prof.addCpu(a, 100);
  prof.addCpu(b, 50);
  prof.addCpu(a, 20);
  prof.setTargetBytes(b, 4096);
  prof.endFrame(out, 1000);
  rhi->endOffscreenFrame();

  auto frames = out.take();
  REQUIRE(frames.size() == 1);
  const auto& f = frames[0];
  REQUIRE(f.frame == 13);
  REQUIRE(f.list == &out);
  REQUIRE(f.cpu_ns == 1000);
  REQUIRE(f.gpu_ns == -1);
  REQUIRE(f.nodes.size() == 2);
  REQUIRE(f.nodes[0].node == 4);
  REQUIRE(f.nodes[0].cpu_ns == 120);
  REQUIRE(f.nodes[0].gpu_ns == -1);
  REQUIRE(f.nodes[1].node == 7);
  REQUIRE(f.nodes[1].target_bytes == 4096);
  REQUIRE(out.take().empty());
}
