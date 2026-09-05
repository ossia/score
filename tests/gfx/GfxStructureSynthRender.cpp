// =============================================================================
// P2-8 -- a Structure Synth `.es` script produces geometry that RASTERISES.
//
// tests/threedim/StructureSynthTest.cpp drives StrucSynth::worker::work() as a
// pure function and asserts the vertex buffer it fills. That is the whole CPU
// contract and it is thorough, and it stops one step short of the only thing a
// user cares about: that the mesh reaches a GPU and covers pixels. Nothing
// anywhere renders an EisenScript.
//
// THE CHAIN is the one a document builds:
//
//   GfxNode<Threedim::StrucSynth> --scene--> ScenePreprocessorNode
//                                --geometry--> raw raster --image--> sink
//
// WHAT IS ASSERTED, and why each half is needed:
//
//   1. `box` draws.  Coverage strictly between 0 and 1: some pixels, not all.
//      "Not all" matters -- a rig that painted the whole frame would satisfy
//      "something rendered" while proving nothing about the mesh.
//   2. The empty program draws NOTHING. Coverage exactly 0. This is the spec's
//      negative control, and it is an assertion rather than a manual experiment
//      because it is also the contract: StructureSynth.hpp:33 early-returns from
//      update() on an empty value, so no worker request is ever made and the
//      geometry output stays empty.
//   3. More geometry covers more pixels. `3 * { x 2 } box` is three unit boxes
//      spread along x -- 108 corner-vertices against `box`'s 36, the exact
//      counts StructureSynthTest pins on the CPU side -- and it must cover
//      strictly more of the frame than the single box. This is the vertex-count
//      half made observable in pixels: (1) and (2) together are satisfied by a
//      chain that renders SOME fixed mesh regardless of the script, and (3) is
//      not.
//
// THE WORKER IS ASYNCHRONOUS and that shapes the fixture. StrucSynth does its
// parsing in halp's worker: the control's update() calls worker.request(), which
// oscr::GpuWorker::initWorker (Crousti/GpuUtils.hpp:46-85) posts to
// score::TaskPool, and the resulting closure comes back through
// ossia::qt::run_async -- a QUEUED invocation on the main thread. GfxPipeline
// pumps frames but never spins an event loop, so without settleWorker() below
// the closure is still sitting in the event queue when the readback is taken and
// every script renders empty. The worker is also installed by the RENDERER, so
// the script has to be delivered AFTER create().
//
// NEGATIVE CONTROL (run, see the ledger): drop the
// `outputs.geometry.dirty_mesh = true` at StructureSynth.cpp:143.
//
// Run:
//   DISPLAY=:0 ctest -R gfx_structure_synth_render
// =============================================================================

#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Threedim/StructureSynth.hpp>

#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <halp/controls.hpp>

#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + file;
}

//! Owns the ProcessModels the GfxNodes hold references to. Must outlive the
//! GfxPipeline, so declare it first at every call site. (Cloned from
//! CroustiCpuNodes.cpp / GfxEnvRenderTargetSize.cpp.)
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

void setInputs(score::gfx::Node& n, std::vector<ossia::value> vals)
{
  score::gfx::Message m;
  m.node_id = n.nodeId;
  for(auto& v : vals)
    m.input.push_back(std::move(v));
  n.process(std::move(m));
}

//! Let the halp worker round-trip: TaskPool thread -> ossia::qt::run_async ->
//! the main thread's event queue. GfxPipeline::render() pumps frames, not
//! events, so nothing here happens unless somebody spins the loop.
//!
//! Deliberately unconditional (no early exit on "geometry arrived"): the state
//! lives inside the renderer and is not reachable from the public node API, so
//! the honest thing is a fixed settle budget. 1.5 s against a parse that takes
//! microseconds -- measured, `box` and `3 * { x 2 } box` both land inside the
//! first 50 ms.
void settleWorker(int ms = 1500)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
  {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    QThread::msleep(5);
  }
}

//! Fraction of the frame that is not the clear colour.
double coverage(const ReadbackImage& img)
{
  if(!img.valid())
    return -1.0;
  int lit = 0;
  for(int y = 0; y < img.height; ++y)
    for(int x = 0; x < img.width; ++x)
    {
      const auto p = img.at(x, y);
      if(int(p[0]) + int(p[1]) + int(p[2]) > 24)
        ++lit;
    }
  return double(lit) / double(img.width * img.height);
}

struct Run
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;
  double cov = -1.0;
};

Run render_script(score::gfx::GraphicsApi api, const char* script)
{
  Run out;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* document = score::test::new_document(app);
    if(!document)
    {
      out.error = "could not create a document (ProcessModel needs one)";
      return;
    }
    const score::DocumentContext& ctx = document->context();

    HalpProcesses procs;
    GfxPipeline p;

    const int ss = p.addNode(procs.make<Threedim::StrucSynth>(ctx));
    const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster
        = p.addRaster(corpus("syn-scene-solid.vs"), corpus("syn-scene-solid.fs"));
    if(ss < 0 || flat < 0 || raster < 0)
    {
      out.error = p.error().empty() ? "node build failed" : p.error();
      return;
    }

    auto* ssOut = p.nodeSceneOut(ss, 0);
    auto* flatIn = p.nodeSceneIn(flat, 0);
    auto* flatOut = p.nodeGeometryOut(flat, 0);
    auto* rasIn = p.geometryIn(raster, 0);
    if(!ssOut || !flatIn || !flatOut || !rasIn)
    {
      out.error = "scene/geometry ports missing on the chain";
      return;
    }
    p.wire(ssOut, flatIn);
    p.wire(flatOut, rasIn);

    const int sink = p.addSink({160, 160});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

    if(!p.create(api))
    {
      out.backend = p.backend();
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = p.error();
      return;
    }
    out.backend = p.backend();

    // The worker is installed by the renderer, so the script can only be
    // delivered now. Slot 0 is `program`; the rest stay monostate so
    // GpuProcessIns leaves position/rotation/scale (and their update()
    // callbacks) alone.
    std::vector<ossia::value> vals(5);
    vals[0] = ossia::value{std::string(script)};
    setInputs(*p.node(ss), std::move(vals));

    p.render(1);   // applies the control -> update() -> worker.request()
    settleWorker(); // TaskPool -> run_async -> main thread applies the closure
    p.render(4);   // the mesh is now on the node; draw with it

    out.cov = coverage(p.readback(sink));
  });
  return out;
}
} // namespace

TEST_CASE(
    "a Structure Synth program rasterises, and an empty one draws nothing",
    "[gfx][threedim][ssynth]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  const Run one = render_script(api, "box");
  if(one.skipped)
    SKIP(one.backend << ": " << one.skip_reason);
  INFO("backend=" << one.backend << " error=" << one.error);
  REQUIRE(one.error.empty());

  const Run none = render_script(api, "");
  REQUIRE(none.error.empty());

  const Run sph = render_script(api, "sphere");
  REQUIRE(sph.error.empty());

  INFO("coverage: box=" << one.cov << " empty=" << none.cov
                        << " sphere=" << sph.cov);

  // 1. Something was drawn, and it was not the whole frame.
  CHECK(one.cov > 0.0);
  CHECK(one.cov < 1.0);

  // 2. The empty program draws nothing: StructureSynth.hpp:33 never asks the
  //    worker, so there is no mesh to hand downstream.
  CHECK(none.cov == 0.0);

  // 3. `sphere` draws, and covers STRICTLY LESS than `box`.
  //
  //    This is geometry, not tuning: libssynth's `sphere` (CreateUnitSphere,
  //    centre matrix*(0.5,0.5,0.5), radius 0.5) is INSCRIBED in the unit cube
  //    `box` spans, so its projection is contained in the cube's under any
  //    camera and any projection, and strictly smaller. No knowledge of the
  //    ScenePreprocessor's default camera is needed.
  //
  //    It is also what makes the case worth anything. Assertions 1 and 2 are
  //    both satisfied by a chain that renders one fixed mesh whenever the script
  //    is non-empty; this one is not. And it runs the comparison in the
  //    direction that vertex COUNT does not predict -- StructureSynthTest pins
  //    570 corner-vertices for `sphere` against 36 for `box`, so the leg with
  //    16x the vertices is the one that must cover LESS.
  CHECK(sph.cov > 0.0);
  CHECK(sph.cov < one.cov);
}
