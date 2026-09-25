// A node cabled from its own depth output into one of its grabbing
// (GrabsFromSource) image inputs (N64 remainder).
//
// A depth or multisampled texture cannot be copied into the previous-frame
// snapshot other self-fed grabbing inputs read, and RenderList used to fall
// back to binding the live texture: the node sampled the depth attachment it
// was rendering into. It must now read the empty 2D texture, and say so once.
//
//   DISPLAY=:0 SCORE_GPU_VALIDATION=0 ctest -R gfx_selffb_depth_grab_l1
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QMutex>
#include <QStringList>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(file);
}

QMutex g_mutex;
QStringList g_warnings;
QtMessageHandler g_previous{};

void capture(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
  if(msg.contains("samples its own"))
  {
    QMutexLocker l{&g_mutex};
    g_warnings.push_back(msg);
    return;
  }
  if(g_previous)
    g_previous(type, ctx, msg);
}
}

TEST_CASE(
    "a node grabbing its own depth output reads an empty texture",
    "[gfx][feedback][selffb][l1]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto cable = GENERATE(
      Process::CableType::DelayedGlutton, Process::CableType::ImmediateGlutton);
  CAPTURE(backend_name(api), int(cable));

  bool skipped = false;
  std::string skip_reason, backend, error;
  std::array<int, 4> px{-1, -1, -1, -1};
  bool boundEmpty = false, boundLive = true, sawRenderList = false;
  QStringList warnings;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    {
      QMutexLocker l{&g_mutex};
      g_warnings.clear();
    }
    g_previous = qInstallMessageHandler(capture);
    GfxPipeline p;
    const int a = p.addRaster(
        corpus("l1selffb-depth-grab.vs"), corpus("l1selffb-depth-grab.fs"));
    const int sink = p.addSink({32, 32});
    if(a < 0)
    {
      error = p.error().empty() ? "build failed" : p.error();
      qInstallMessageHandler(g_previous);
      return;
    }
    p.wire(p.imageOut(a, 1), p.imageIn(a, 0), cable);
    p.wire(p.imageOut(a, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      skip_reason = p.skipReason();
      error = skipped ? std::string{} : p.error();
      qInstallMessageHandler(g_previous);
      return;
    }
    backend = p.backend();
    p.render(6);
    const auto img = p.readback(sink);
    if(img.valid())
    {
      const auto c = img.center();
      px = {c[0], c[1], c[2], c[3]};
    }

    auto* node = p.isf(a);
    for(auto& [rl, r] : node->renderedNodes)
    {
      sawRenderList = true;
      auto* bound = rl->selfFeedbackGrab(*p.imageIn(a, 0));
      auto* live = r->textureForOutput(*p.imageOut(a, 1));
      boundEmpty = bound == &rl->emptyTexture();
      boundLive = bound == live || bound == nullptr;
    }
    if(error.empty())
      error = p.error();
    qInstallMessageHandler(g_previous);
    QMutexLocker l{&g_mutex};
    warnings = g_warnings;
  });
  if(skipped)
    SKIP(skip_reason);
  REQUIRE(error.empty());
  CAPTURE(backend, px, warnings.join("\n").toStdString());

  REQUIRE(sawRenderList);
  CHECK(boundEmpty);
  CHECK_FALSE(boundLive);
  REQUIRE(warnings.size() == 1);
  CHECK(warnings[0].contains("depth"));
  CHECK(px[0] <= 4);
  CHECK(px[1] >= 60);
  CHECK(px[1] <= 68);
}
