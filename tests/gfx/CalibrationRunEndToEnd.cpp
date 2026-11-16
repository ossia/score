// The projector calibration tool end to end, with no process and no playing:
// CalibrationRun shows the patterns on a multi-window node's windows, a fake
// camera films those windows, and the calibrations and blend maps come out.

#include "MultiWindowTestCommon.hpp"

#include <score_test/Document.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RowOrder.hpp>
#include <Gfx/Settings/Model.hpp>
#include <Threedim/CalibrationRun.hpp>

#include <core/document/Document.hpp>

#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstring>
#include <thread>

using namespace score::test;
using namespace score::test::gfx;
using namespace score::test::gfx::multiwindow;

namespace
{
constexpr int Outputs = 3;
constexpr int PW = 96, PH = 64;
constexpr int CW = 3 * 120, CH = 100;

//! Films the calibration images of the windows: output k fills a third of the
//! frame, side by side, as three projectors on one wall would.
struct FakeCameraNode final : score::gfx::ProcessNode
{
  score::gfx::MultiWindowNode* windows{};
  FakeCameraNode() { output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}}); }

  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList&) const noexcept override
  {
    struct Renderer final : score::gfx::NodeRenderer
    {
      using NodeRenderer::NodeRenderer;
      QRhiTexture* m_tex{};
      std::vector<float> m_img, m_up;

      void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch&) override
      {
        m_tex = r.state.rhi->newTexture(QRhiTexture::RGBA32F, QSize{CW, CH}, 1);
        m_tex->create();
        m_img.assign(std::size_t(CW) * CH * 4, 0.f);
        m_up = m_img;
        m_initialized = true;
      }
      void update(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res, score::gfx::Edge*) override
      {
        const auto* w = static_cast<const FakeCameraNode&>(node).windows;
        if(!w)
          return;
        const auto& outs = w->windowOutputs();
        const int panel = CW / Outputs;
        for(int y = 0; y < CH; y++)
          for(int x = 0; x < CW; x++)
          {
            const int k = x / panel;
            float v = 0.02f;
            if(k < int(outs.size()) && w->calibrating())
              if(const auto& img = outs[k].calibration; img && img->width > 0)
              {
                const int u = (x - k * panel) * img->width / panel;
                const int t = y * img->height / CH;
                v += 0.8f * img->rgba[(std::size_t(t) * img->width + u) * 4] / 255.f;
              }
            float* p = &m_img[(std::size_t(y) * CW + x) * 4];
            p[0] = p[1] = p[2] = v;
            p[3] = 1.f;
          }
        const float* src = m_img.data();
        if(score::gfx::uploadReversesRows(*r.state.rhi))
        {
          for(int y = 0; y < CH; y++)
            std::memcpy(
                &m_up[std::size_t(y) * CW * 4], &m_img[std::size_t(CH - 1 - y) * CW * 4],
                CW * 4 * sizeof(float));
          src = m_up.data();
        }
        res.uploadTexture(
            m_tex, QRhiTextureUploadDescription{
                       {0, 0, QRhiTextureSubresourceUploadDescription{
                                  src, quint32(m_img.size() * sizeof(float))}}});
      }
      QRhiTexture* textureForOutput(const score::gfx::Port&) override { return m_tex; }
      void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
      void release(score::gfx::RenderList&) override
      {
        delete m_tex;
        m_tex = nullptr;
      }
    };
    return new Renderer{*this};
  }
};
}

TEST_CASE(
    "CalibrationRun calibrates every output of a multi-window node without a process",
    "[threedim][calibrationwizard][gui]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString base = dir.filePath("hall");

  std::vector<Gfx::OutputMapping> mappings;
  for(int i = 0; i < Outputs; i++)
  {
    Gfx::OutputMapping m;
    m.sourceRect = QRectF{0, 0, 1, 1};
    m.windowPosition = {20 + i * 110, 40};
    m.windowSize = {PW, PH};
    m.color.gain = {0.5f, 0.5f, 0.5f};
    mappings.push_back(m);
  }

  std::string skip, failure;
  Gfx::WindowSettings assigned;
  bool done = false, clearedAfter = false, calibratingDuring = false;
  int resultsOk = 0;
  std::vector<float> coverage;
  run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    auto& settings = ctx.settings<Gfx::Settings::Model>();
    settings.setVSync(false);
    MultiWindowRig rig;
    if(!rig.build(score::gfx::OpenGL, mappings))
    {
      skip = rig.skipped() ? rig.skipReason() : rig.error();
      return;
    }
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& g = doc->context().plugin<Gfx::DocumentPlugin>().context;
    auto cam = std::make_unique<FakeCameraNode>();
    cam->windows = rig.node.get();
    const int32_t camId = g.register_node(std::move(cam));

    Gfx::WindowSettings ws;
    ws.mode = Gfx::WindowMode::MultiWindow;
    ws.outputs = mappings;
    ws.inputWidth = PW;
    ws.inputHeight = PH;

    {
      Threedim::CalibrationRun::Setup setup;
      setup.gfx = &g;
      auto* node = rig.node.get();
      setup.node = [node] { return node; };
      setup.camera = Gfx::port_index{camId, 0};
      setup.plan = Threedim::planCalibration(ws, base);
      setup.settings.mode = Threedim::StructuredLightSession::ViewerCamera;
      setup.settings.settleMs = 60;
      setup.settings.frames = 1;
      setup.blend = true;
      setup.canvas.projection = Gfx::CanvasProjection::Equirectangular;
      setup.sourceRects.assign(Outputs, QRectF{0, 0, 1, 1});
      Threedim::CalibrationRun run{std::move(setup)};

      const auto now = [] { return double(QDateTime::currentMSecsSinceEpoch()); };
      REQUIRE(run.start(now()));
      calibratingDuring = rig.node->calibrating();
      const auto deadline = now() + 240000.;
      while(run.running() && now() < deadline)
      {
        rig.render(1);
        g.updateGraph();
        for(auto* out : g.outputs())
        {
          out->render();
          if(auto rs = out->renderState(); rs && rs->rhi)
            rs->rhi->finish();
        }
        run.tick(now());
        if(run.state() == Threedim::CalibrationRun::State::Blending)
          std::this_thread::sleep_for(std::chrono::milliseconds{5});
      }
      done = run.state() == Threedim::CalibrationRun::State::Done;
      failure = run.status().toStdString();
      for(const auto& l : run.takeLog())
        failure += " | " + l.toStdString();
      for(const auto& r : run.session().results())
      {
        resultsOk += r && r->ok;
        coverage.push_back(r ? r->coverage : 0.f);
      }
      clearedAfter = !rig.node->calibrating();
      assigned = Threedim::assignCalibrations(ws, run.setup().plan.names, 0);
    }
    g.unregister_node(camId);
    g.updateGraph();
  });

  if(!skip.empty())
    SKIP(skip);
  INFO(failure);
  CHECK(calibratingDuring);
  REQUIRE(done);
  CHECK(clearedAfter);
  CHECK(resultsOk == Outputs);
  for(float c : coverage)
    CHECK(c > 80.f);
  for(const QString& n : QStringList{base, base + "_p1", base + "_p2"})
  {
    INFO(n.toStdString());
    CHECK(QFile::exists(n + ".json"));
    CHECK(QFile::exists(n + ".xyz.pfm"));
    CHECK(QFile::exists(n + ".alpha.pfm"));
    CHECK(QFile::exists(n + ".warp.pfm"));
    CHECK(QFile::exists(n + ".warp.valid.pfm"));
  }
  REQUIRE(assigned.outputs.size() == std::size_t(Outputs));
  CHECK(assigned.outputs[0].maps.warp == base + ".warp.pfm");
  CHECK(assigned.outputs[2].maps.warp == base + "_p2.warp.pfm");
  CHECK(assigned.outputs[1].maps.alpha == base + "_p1.alpha.pfm");
}
