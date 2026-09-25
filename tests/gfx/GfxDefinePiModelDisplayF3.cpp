// C6: a geometry filter's own #define PI does not break ModelDisplay.
//
// ModelDisplay splices every geometry filter's source into its vertex shader
// ahead of the projection code. The fulldome projection once declared
// `const float PI`, so a filter that #defines PI (as the Ashima noise header in
// cute-curl does) turned it into `const float 3.1415926535 = ...` and the vertex
// stage failed to bake: nothing drew. tests/threedim/f3c6-define-pi-filter.glsl
// (outside the corpus, which pins its geometry filter count) scales every
// vertex by sin(PI * k); through a fulldome ModelDisplay, k = 0.5 must draw the
// textured cube with no bake failure, and k = 1 must draw nothing, which proves
// the filter code is really in the stage.
//
// Registration: see the test_gfx_define_pi_model_display_f3 target.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Gfx/Graph/GeometryFilterNode.hpp>

#include <Threedim/ModelDisplay/ModelDisplayNode.hpp>
#include <Threedim/Primitive.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QFile>
#include <QMutex>

#include <isf.hpp>

#include <string>
#include <vector>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

QMutex g_logMutex;
std::vector<QString> g_log;
QtMessageHandler g_prevHandler{};

void captureHandler(QtMsgType t, const QMessageLogContext& ctx, const QString& msg)
{
  {
    QMutexLocker lock{&g_logMutex};
    g_log.push_back(msg);
  }
  if(g_prevHandler)
    g_prevHandler(t, ctx, msg);
}

struct LogCapture
{
  LogCapture()
  {
    QMutexLocker lock{&g_logMutex};
    g_log.clear();
    g_prevHandler = qInstallMessageHandler(captureHandler);
  }
  ~LogCapture()
  {
    qInstallMessageHandler(g_prevHandler);
    g_prevHandler = {};
  }
  static int count(const QString& needle)
  {
    QMutexLocker lock{&g_logMutex};
    int n = 0;
    for(const auto& m : g_log)
      if(m.contains(needle, Qt::CaseInsensitive))
        n++;
    return n;
  }
};

struct HalpProcesses
{
  std::vector<std::unique_ptr<Process::ProcessModel>> models;
  int next = 1;
  template <typename T>
  std::unique_ptr<score::gfx::Node> make(const score::DocumentContext& ctx)
  {
    auto model = std::make_unique<oscr::ProcessModel<T>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{next}, ctx, nullptr);
    auto* raw = model.get();
    models.push_back(std::move(model));
    return std::unique_ptr<score::gfx::Node>{
        new oscr::GfxNode<T>{*raw, {}, Gfx::exec_controls{}, next++, ctx}};
  }
};

std::unique_ptr<score::gfx::GeometryFilterNode>
makeFilter(const QString& path, int index, std::string& err)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
  {
    err = "cannot open " + path.toStdString();
    return {};
  }
  try
  {
    ::isf::parser p{
        QString::fromUtf8(f.readAll()).toStdString(),
        ::isf::parser::ShaderType::GeometryFilter};
    return std::make_unique<score::gfx::GeometryFilterNode>(
        int64_t(index), p.data(), QString::fromStdString(p.geometry_filter()));
  }
  catch(const std::exception& e)
  {
    err = std::string("geometry filter parse failed: ") + e.what();
  }
  return {};
}

struct Shot
{
  bool skipped{};
  std::string error;
  int coloured = 0;
  int bakeFailures = 0;
};

Shot shoot(score::gfx::GraphicsApi api, float k)
{
  Shot s;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    LogCapture capture;
    auto* doc = new_document(app);
    if(!doc)
    {
      s.error = "no document";
      return;
    }
    HalpProcesses procs;
    GfxPipeline p;
    const int image = p.addIsf(corpus("syn-red-over-blue.fs"));
    const int cube = p.addNode(procs.make<Threedim::Cube>(doc->context()));
    auto filter = makeFilter(
        QStringLiteral(F3_THREEDIM_FIXTURE_DIR "/f3c6-define-pi-filter.glsl"), 7, s.error);
    if(!filter)
      return;
    auto* filterNode = filter.get();
    const int filt = p.addNode(std::move(filter));
    auto md = std::make_unique<score::gfx::ModelDisplayNode>();
    md->texture_projection = 0;
    md->camera_mode = 1;
    md->position = {0.f, 0.f, 2.5f};
    md->center = {0.f, 0.f, 0.f};
    md->fov = 90.f;
    auto* mdNode = md.get();
    const int display = p.addNode(std::move(md));
    if(image < 0 || cube < 0 || filt < 0 || display < 0)
    {
      s.error = "node build failed: " + p.error();
      return;
    }
    auto* fin = p.nodeGeometryIn(filt, 0);
    auto* fout = p.nodeGeometryOut(filt, 0);
    if(!fin || !fout || filterNode->input.size() < 2)
    {
      s.error = "the geometry filter has no geometry ports or no k control";
      return;
    }
    p.wire(p.imageOut(image, 0), mdNode->input[0]);
    p.wire(p.nodeSceneOut(cube, 0), fin);
    p.wire(fout, mdNode->input[1]);
    static_cast<score::gfx::ProcessNode&>(*filterNode).process(int32_t(1), ossia::value{k});
    const int sink = p.addSink({96, 96});
    p.wire(p.nodeImageOut(display, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      s.skipped = p.skipped();
      s.error = s.skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    const auto img = p.readback(sink);
    if(!img.valid())
    {
      s.error = "empty readback";
      return;
    }
    for(int y = 0; y < img.height; y++)
      for(int x = 0; x < img.width; x++)
      {
        const auto px = img.at(x, y);
        if((px[0] > 128 && px[2] < 64) || (px[2] > 128 && px[0] < 64))
          s.coloured++;
      }
    s.bakeFailures = LogCapture::count(QStringLiteral("bake failed"));
  });
  return s;
}
}

TEST_CASE(
    "a geometry filter's #define PI does not break a fulldome ModelDisplay",
    "[gfx][threedim][modeldisplay][geometryfilter]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Shot kept = shoot(api, 0.5f);
  if(kept.skipped)
    SKIP("backend unavailable");
  INFO("error=" << kept.error);
  REQUIRE(kept.error.empty());
  CHECK(kept.bakeFailures == 0);
  CHECK(kept.coloured > 100);

  const Shot collapsed = shoot(api, 1.0f);
  INFO("error=" << collapsed.error);
  REQUIRE(collapsed.error.empty());
  CHECK(collapsed.bakeFailures == 0);
  CHECK(collapsed.coloured == 0);
}
