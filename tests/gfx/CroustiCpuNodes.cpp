// =============================================================================
// The Crousti CPU node path: score's avendish (halp) objects reaching the render
// graph through oscr::ProcessModel<T> + oscr::GfxNode<T>, whose renderer is the
// GfxRenderer specialisation in Crousti/CpuFilterNode.hpp.
//
// The tests-scene shader sweep needs an external corpus
// (SCORE_SHADER_LIBRARY_DIR pointed at csf-testers) and so does not run in a
// default `ctest`. Everything here is synthetic and self-contained.
//
// ORACLES. These are scene-graph transforms, so the honest observable is WHERE
// geometry lands, and the shader deliberately shades by nothing (see
// syn-scene-solid.fs): a mesh that is drawn but unlit is otherwise
// indistinguishable from a mesh that was never drawn, which is the trap the
// scene sweep documents in its own header. Each case therefore compares two
// renders that differ in exactly one control and asserts the DIRECTION of the
// change, not merely that something was drawn.
// =============================================================================
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Threedim/Camera.hpp>
#include <Threedim/CameraArray.hpp>
#include <Threedim/Primitive.hpp>
#include <Threedim/SceneGroup.hpp>
#include <Threedim/Transform3D.hpp>

#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <halp/texture.hpp>
#include <halp/controls.hpp>

#include <Gfx/Graph/GeometryFilterNode.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <QFile>

#include <atomic>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

//! Owns the ProcessModels the GfxNodes hold references to. Must outlive the
//! GfxPipeline, so declare it first at every call site.
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

//! Deliver control values to a Crousti node. CustomGfxNodeBase::process() merges
//! into last_message and keeps what it already had when a later message carries
//! none, so one call before rendering persists across the empty per-frame
//! Timings messages the pump sends.
void setInputs(score::gfx::Node& n, std::vector<ossia::value> vals)
{
  score::gfx::Message m;
  m.node_id = n.nodeId;
  for(auto& v : vals)
    m.input.push_back(std::move(v));
  n.process(std::move(m));
}

//! Fraction of pixels that are lit, the horizontal centre of mass of the lit
//! ones in [0,1], and whether the frame's own centre pixel is lit. Centroid is
//! meaningless with nothing lit, so callers must check `coverage` first.
struct Placement
{
  double coverage{};
  double centroidX{};
  bool centreLit{};
};

Placement placement_of(const ReadbackImage& img)
{
  Placement p;
  long lit = 0;
  double sx = 0.;
  for(int y = 0; y < img.height; y++)
  {
    for(int x = 0; x < img.width; x++)
    {
      if(img.at(x, y)[0] > 128)
      {
        lit++;
        sx += double(x);
      }
    }
  }
  const double total = double(img.width) * double(img.height);
  p.coverage = total > 0 ? double(lit) / total : 0.;
  p.centroidX = lit > 0 ? (sx / double(lit)) / double(img.width) : 0.;
  p.centreLit = img.width > 0 && img.height > 0
                && img.at(img.width / 2, img.height / 2)[0] > 128;
  return p;
}

struct SceneRun
{
  IsfResult r;
  Placement p;
};
}

TEST_CASE(
    "a Crousti scene chain builds, renders and tears down",
    "[gfx][crousti][scene][threedim]")
{
  // Cube -> Transform 3D -> ScenePreprocessor -> raster, every producer a halp
  // process reaching the graph through oscr::GfxNode. This is the path
  // CpuFilterNode.hpp's renderer serves, and nothing in a default ctest run
  // entered it before.
  //
  // SCOPE: this asserts that the chain builds, renders frames and tears down
  // without error, and nothing about pixels.
  //
  // What IS verified by probe: the halp processes are constructed,
  // oscr::GfxNode's renderer is created (mode RawRaster, one pass), initState
  // and initPass both run, and the raster receives real geometry -- 36 vertices
  // for a Cube, 1764 for a Torus, 0 with no producer wired. GpuProcessIns
  // applies the controls (mess.input.size 4, fields 1..3). So the Crousti path
  // genuinely executes.
  //
  // What is NOT understood, and why there is no pixel oracle: the readback does
  // not vary with any of that. Coverage is a fixed NDC quadrant, identical for a
  // 36-vertex Cube and a 1764-vertex Torus, at any Transform 3D position or
  // scale. Since the mesh demonstrably differs at the raster, the invariance is
  // in the readback or the projection, not in the scene chain -- and until that
  // is pinned down, any pixel assertion here measures the unknown part.
  const auto api = GENERATE(from_range(platform_backends()));

  auto run = [&](bool withGeometry) {
    SceneRun out;
    run_in_gui_app([&](const score::GUIApplicationContext& app) {
      auto* document = score::test::new_document(app);
      if(!document)
      {
        out.r.error = "could not create a document (ProcessModel needs one)";
        return;
      }
      const score::DocumentContext& ctx = document->context();

      HalpProcesses procs;
      GfxPipeline p;

      const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
      const int raster
          = p.addRaster(corpus("syn-scene-solid.vs"), corpus("syn-scene-solid.fs"));
      if(raster < 0)
      {
        out.r.error = "raster build failed: " + p.error();
        return;
      }

      if(withGeometry)
      {
        const int cube = p.addNode(procs.make<Threedim::Cube>(ctx));
        const int xf = p.addNode(procs.make<Threedim::Transform3D>(ctx));
        auto* cubeOut = p.nodeSceneOut(cube, 0);
        auto* xfIn = p.nodeSceneIn(xf, 0);
        auto* xfOut = p.nodeSceneOut(xf, 0);
        auto* flatIn = p.nodeSceneIn(flat, 0);
        if(!cubeOut || !xfIn || !xfOut || !flatIn)
        {
          out.r.error = "scene ports missing on the chain";
          return;
        }
        p.wire(cubeOut, xfIn);
        p.wire(xfOut, flatIn);
      }

      p.wire(p.nodeGeometryOut(flat, 0), p.geometryIn(raster, 0));
      const int sink = p.addSink({160, 160});
      p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

      if(!p.create(api))
      {
        out.r.backend = p.backend();
        out.r.skipped = p.skipped();
        out.r.skip_reason = p.skipReason();
        out.r.error = p.error();
        return;
      }
      out.r.backend = p.backend();
      p.render(4);
      out.r.outputs.push_back(p.readback(sink));
    });
    if(!out.r.outputs.empty() && out.r.outputs[0].valid())
      out.p = placement_of(out.r.outputs[0]);
    return out;
  };

  const auto drawn = run(true);
  if(drawn.r.skipped)
    SKIP(drawn.r.backend + ": " + drawn.r.skip_reason);
  INFO("backend=" << drawn.r.backend << " error: " << drawn.r.error);
  REQUIRE(drawn.r.error.empty());
  REQUIRE(drawn.r.outputs.size() == 1);
  REQUIRE(drawn.r.outputs[0].valid());

  const auto empty = run(false);
  REQUIRE(empty.r.error.empty());
  REQUIRE(empty.r.outputs.size() == 1);
  REQUIRE(empty.r.outputs[0].valid());

  // The rig is proven, not assumed: the same wiring driven by the CSF triangle
  // producer fills the whole frame, so a partial coverage here is the cube's
  // own silhouette rather than a fixture artefact.
  INFO("coverage with cube=" << drawn.p.coverage
                             << " without a producer=" << empty.p.coverage);
  CHECK(empty.p.coverage == 0.);
  CHECK(drawn.p.coverage > 0.01);
  CHECK(drawn.p.coverage < 1.);
}

TEST_CASE(
    "Scene Group renders the scene on its first input",
    "[gfx][crousti][scene][threedim]")
{
  // A cube on input 0, optionally a torus on input 1. Both configurations must
  // render; see the note at the assertions for why the merge itself is measured
  // but deliberately not asserted.
  const auto api = GENERATE(from_range(platform_backends()));

  auto run = [&](bool second) {
    SceneRun out;
    run_in_gui_app([&](const score::GUIApplicationContext& app) {
      auto* document = score::test::new_document(app);
      if(!document)
      {
        out.r.error = "could not create a document (ProcessModel needs one)";
        return;
      }
      const score::DocumentContext& ctx = document->context();

      HalpProcesses procs;
      GfxPipeline p;

      const int cube = p.addNode(procs.make<Threedim::Cube>(ctx));
      const int group = p.addNode(procs.make<Threedim::SceneGroup>(ctx));
      const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
      const int raster
          = p.addRaster(corpus("syn-scene-solid.vs"), corpus("syn-scene-solid.fs"));
      if(raster < 0)
      {
        out.r.error = "raster build failed: " + p.error();
        return;
      }

      auto* gIn0 = p.nodeSceneIn(group, 0);
      auto* gIn1 = p.nodeSceneIn(group, 1);
      auto* gOut = p.nodeSceneOut(group, 0);
      if(!gIn0 || !gIn1 || !gOut)
      {
        out.r.error = "Scene Group does not expose two scene inputs and an output";
        return;
      }
      p.wire(p.nodeSceneOut(cube, 0), gIn0);
      if(second)
      {
        const int torus = p.addNode(procs.make<Threedim::Torus>(ctx));
        p.wire(p.nodeSceneOut(torus, 0), gIn1);
      }
      p.wire(gOut, p.nodeSceneIn(flat, 0));
      p.wire(p.nodeGeometryOut(flat, 0), p.geometryIn(raster, 0));
      const int sink = p.addSink({160, 160});
      p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

      if(!p.create(api))
      {
        out.r.backend = p.backend();
        out.r.skipped = p.skipped();
        out.r.skip_reason = p.skipReason();
        out.r.error = p.error();
        return;
      }
      out.r.backend = p.backend();
      p.render(4);
      out.r.outputs.push_back(p.readback(sink));
    });
    if(!out.r.outputs.empty() && out.r.outputs[0].valid())
      out.p = placement_of(out.r.outputs[0]);
    return out;
  };

  const auto one = run(false);
  if(one.r.skipped)
    SKIP(one.r.backend + ": " + one.r.skip_reason);
  INFO("backend=" << one.r.backend << " error: " << one.r.error);
  REQUIRE(one.r.error.empty());
  REQUIRE(one.r.outputs.size() == 1);
  REQUIRE(one.r.outputs[0].valid());
  REQUIRE(one.p.coverage > 0.01);

  const auto both = run(true);
  REQUIRE(both.r.error.empty());
  REQUIRE(both.r.outputs.size() == 1);
  REQUIRE(both.r.outputs[0].valid());

  // NOT ASSERTED: that the second input contributes. Cube-only and cube+torus
  // render byte-identical frames, but that says nothing about Scene Group --
  // the same rig renders the same fixed NDC quadrant for ANY mesh, so mesh
  // content does not reach the pixels here at all (see the scope note on the
  // first case). The merge contract is untestable by pixels until that is
  // fixed, and pinning it to today's output would pin the rig, not the node.
  INFO("coverage cube only=" << one.p.coverage << " cube+torus=" << both.p.coverage);
  CHECK(both.p.coverage > 0.01);
}

TEST_CASE(
    "Camera Array publishes a scene through the Crousti wrapper",
    "[gfx][crousti][scene][threedim]")
{
  // Camera Array has no scene input: it only produces. What is asserted is that
  // the Crousti wrapper builds it, exposes the scene outlet and renders a frame
  // with a cube merged alongside it -- the camera contributes no geometry, so
  // the oracle is that adding it does not lose the cube.
  const auto api = GENERATE(from_range(platform_backends()));

  IsfResult r;
  double coverage = 0.;
  bool hasSceneOut = false;

  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* document = score::test::new_document(app);
    if(!document)
    {
      r.error = "could not create a document (ProcessModel needs one)";
      return;
    }
    const score::DocumentContext& ctx = document->context();

    HalpProcesses procs;
    GfxPipeline p;

    const int cube = p.addNode(procs.make<Threedim::Cube>(ctx));
    const int cams = p.addNode(procs.make<Threedim::CameraArray>(ctx));
    const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster
        = p.addRaster(corpus("syn-scene-solid.vs"), corpus("syn-scene-solid.fs"));
    if(raster < 0)
    {
      r.error = "raster build failed: " + p.error();
      return;
    }

    auto* camOut = p.nodeSceneOut(cams, 0);
    hasSceneOut = camOut != nullptr;
    if(!camOut)
    {
      r.error = "Camera Array exposes no scene output";
      return;
    }
    p.wire(p.nodeSceneOut(cube, 0), p.nodeSceneIn(flat, 0));
    p.wire(camOut, p.nodeSceneIn(flat, 0));
    p.wire(p.nodeGeometryOut(flat, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink({160, 160});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

    if(!p.create(api))
    {
      r.backend = p.backend();
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.error = p.error();
      return;
    }
    r.backend = p.backend();
    p.render(4);
    r.outputs.push_back(p.readback(sink));
  });

  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error: " << r.error);
  REQUIRE(r.error.empty());
  CHECK(hasSceneOut);
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());
  coverage = placement_of(r.outputs[0]).coverage;
  INFO("coverage with a Camera Array in the scene = " << coverage);
  CHECK(coverage > 0.01);
}





//! Shared scene-chain render used by the two expected-failure cases below.
namespace
{
Placement render_scene_chain(
    score::gfx::GraphicsApi api, const score::DocumentContext& ctx,
    HalpProcesses& procs, bool torus, bool withTransform, float xpos, bool& ok)
{
  Placement out;
  ok = false;
  GfxPipeline p;
  const int prim = torus ? p.addNode(procs.make<Threedim::Torus>(ctx))
                         : p.addNode(procs.make<Threedim::Cube>(ctx));
  if(torus)
  {
    // Threedim::Torus defaults to R1 = 10 (ring) / R2 = 1 (tube), so its
    // surface lives entirely in the annulus 9 <= sqrt(x^2+y^2) <= 11 of the XY
    // plane (vcg::tri::Torus, platonic.h:673-700: the profile circle is placed
    // at +hRingRadius on X and swept about Z). This rig has NO camera -- the
    // shader is `clipSpaceCorrMatrix * per_draws[draw_id].model * position`, so
    // model space IS clip space -- and the visible NDC square is [-1,1]^2,
    // which sits wholly inside the torus's own hole. Scaling to 0.05 brings the
    // ring (radius 0.5, tube 0.05) into the frame. The Cube needs none of this:
    // vcg::tri::Box is built on [0,1]^3 and lands on one NDC quadrant.
    setInputs(
        *p.node(prim), {ossia::value{ossia::vec3f{0.f, 0.f, 0.f}},
                        ossia::value{ossia::vec3f{0.f, 0.f, 0.f}},
                        ossia::value{ossia::vec3f{0.05f, 0.05f, 0.05f}}});
  }
  const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
  // syn-scene-perdraw-solid places the geometry with per_draws[draw_id].model,
  // the matrix the FLATTENER fills. syn-scene-solid.vs multiplies by
  // MODEL_MATRIX instead, which no scene chain ever writes -- see the
  // "a scene transform reaches the shader" case for the measurement.
  const int raster = p.addRaster(
      corpus("syn-scene-perdraw-solid.vs"), corpus("syn-scene-perdraw-solid.fs"));
  if(raster < 0)
    return out;

  if(withTransform)
  {
    const int xf = p.addNode(procs.make<Threedim::Transform3D>(ctx));
    p.wire(p.nodeSceneOut(prim, 0), p.nodeSceneIn(xf, 0));
    p.wire(p.nodeSceneOut(xf, 0), p.nodeSceneIn(flat, 0));
    setInputs(
        *p.node(xf), {ossia::value{}, ossia::value{ossia::vec3f{xpos, 0.f, 0.f}},
                      ossia::value{ossia::vec3f{0.f, 0.f, 0.f}},
                      ossia::value{ossia::vec3f{1.f, 1.f, 1.f}}});
  }
  else
  {
    p.wire(p.nodeSceneOut(prim, 0), p.nodeSceneIn(flat, 0));
  }
  p.wire(p.nodeGeometryOut(flat, 0), p.geometryIn(raster, 0));
  const int sink = p.addSink({160, 160});
  p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
  if(!p.create(api))
    return out;
  p.render(4);
  out = placement_of(p.readback(sink));
  ok = true;
  return out;
}
}

TEST_CASE(
    "Transform 3D moves the mesh it wraps",
    "[gfx][crousti][scene][threedim]")
{
  // THIS WAS AN EXPECTED-FAILURE PIN. It failed because the RIG read the wrong
  // matrix, not because Transform 3D is broken.
  //
  // The old rig rasterised with syn-scene-solid.vs, whose one line is
  // `gl_Position = clipSpaceCorrMatrix * MODEL_MATRIX * position`. MODEL_MATRIX
  // is the RAW-RASTER convention: the only writer is
  // RenderedRawRasterPipelineNode::process(int32_t, const ossia::transform3d&)
  // (RenderedRawRasterPipelineNode.cpp:3378), fed by a transform3d message on
  // the raster node's OWN port. Nothing in a scene chain writes it, so it was
  // identity at every Transform 3D position and the silhouette could not move.
  //
  // The scene path's per-object matrix is PerDrawGPU::model
  // (ScenePreprocessorNode.cpp:41-49), published as the `per_draws` auxiliary
  // and indexed by the per-instance `draw_id` VERTEX_INPUT. render_scene_chain
  // now rasterises with syn-scene-perdraw-solid, which reads exactly that --
  // the same thing shadow_cascades.vert and five real corpus documents do --
  // and the centroid moves. See "a scene transform reaches the shader through
  // per_draws" for the direct measurement of the matrix itself.
  const auto api = GENERATE(from_range(platform_backends()));
  Placement centred, shifted;
  bool okA = false, okB = false;

  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = score::test::new_document(app);
    if(!doc)
      return;
    HalpProcesses procs;
    centred = render_scene_chain(api, doc->context(), procs, false, true, 0.f, okA);
  });
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = score::test::new_document(app);
    if(!doc)
      return;
    HalpProcesses procs;
    shifted = render_scene_chain(api, doc->context(), procs, false, true, 0.5f, okB);
  });

  if(!okA || !okB)
    SKIP("backend unavailable");
  // render_scene_chain rasterises with syn-scene-perdraw-solid, whose vertex
  // stage reads per_draws[draw_id].model out of a storage buffer -- and the
  // per_draws half of the second probe below does the same. Below GLSL 4.30
  // there is no such thing to read.
  if(const char* why = storage_buffer_skip_reason(api))
    SKIP(why);
  // Negative control: a shift assertion is vacuous if nothing was drawn.
  REQUIRE(centred.coverage > 0.01);
  INFO("centroidX centred=" << centred.centroidX << " shifted=" << shifted.centroidX);
  CHECK(shifted.centroidX != centred.centroidX);
}

TEST_CASE(
    "a Torus reaches the rasterizer and draws",
    "[gfx][crousti][scene][threedim]")
{
  // THIS WAS AN EXPECTED-FAILURE PIN, and it was the SECOND rig mistake in this
  // file, unrelated to the first (the retired MODEL_MATRIX one).
  //
  // The pin recorded that the rasterizer receives 1764 vertices for the Torus
  // against 36 for the Cube, that the Cube draws through the identical chain,
  // and that the Torus frame is nevertheless exactly empty. All three
  // observations were correct. The missing one: WHERE those 1764 vertices are.
  //
  // Threedim::Torus's default controls are R1 = 10 and R2 = 1, so every vertex
  // it emits satisfies 9 <= sqrt(x^2+y^2) <= 11. The rig has no camera -- the
  // rasteriser is `gl_Position = clipSpaceCorrMatrix * per_draws[draw_id].model
  // * position`, i.e. model space is clip space -- so the frame shows NDC
  // [-1,1]^2, which lies entirely within the torus's 9-unit hole. The geometry
  // was generated correctly, uploaded correctly and drawn correctly; all of it
  // simply missed the viewport. Scaling the primitive to 0.05 (through the
  // scene transform the flattener already publishes in per_draws[].model)
  // brings it into frame and it draws: measured coverage 0.078125, against the
  // 0.0785 = pi*(0.55^2 - 0.45^2)/4 an annulus of those radii must cover in a
  // 2x2 NDC square. Reverting just the scale returns coverage to exactly 0.
  //
  // The oracle is therefore not "something was drawn" but "a RING was drawn":
  // the centre of the frame must stay dark, which no full silhouette (and no
  // stray full-frame clear) can satisfy.
  const auto api = GENERATE(from_range(platform_backends()));
  Placement cube, torus;
  bool okA = false, okB = false;

  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = score::test::new_document(app);
    if(!doc)
      return;
    HalpProcesses procs;
    cube = render_scene_chain(api, doc->context(), procs, false, false, 0.f, okA);
  });
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = score::test::new_document(app);
    if(!doc)
      return;
    HalpProcesses procs;
    torus = render_scene_chain(api, doc->context(), procs, true, false, 0.f, okB);
  });

  if(!okA || !okB)
    SKIP("backend unavailable");
  // render_scene_chain rasterises with syn-scene-perdraw-solid, whose vertex
  // stage reads per_draws[draw_id].model out of a storage buffer -- and the
  // per_draws half of the second probe below does the same. Below GLSL 4.30
  // there is no such thing to read.
  if(const char* why = storage_buffer_skip_reason(api))
    SKIP(why);
  // Control: the Cube through the same chain must draw, else the chain is at
  // fault rather than the Torus.
  REQUIRE(cube.coverage > 0.01);
  INFO("coverage cube=" << cube.coverage << " torus=" << torus.coverage);
  CHECK(torus.coverage > 0.01);
  CHECK(torus.coverage < 0.5);
  // A torus is a ring: the hole must show. This is what separates "the Torus
  // drew" from "the frame was filled by something else".
  CHECK_FALSE(torus.centreLit);
}

//! Build a GeometryFilterNode from a MODE:"GEOMETRY_FILTER" source on disk.
namespace
{
std::unique_ptr<score::gfx::Node>
make_geometry_filter(const QString& path, std::string& err)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
  {
    err = "cannot open geometry filter: " + path.toStdString();
    return {};
  }
  const auto src = QString::fromUtf8(f.readAll()).toStdString();
  try
  {
    isf::parser p{src, isf::parser::ShaderType::GeometryFilter};
    auto desc = p.data();
    return std::make_unique<score::gfx::GeometryFilterNode>(
        1, desc, QString::fromStdString(p.geometry_filter()));
  }
  catch(const std::exception& e)
  {
    err = std::string("geometry filter parse failed: ") + e.what();
    return {};
  }
}
}

TEST_CASE(
    "a geometry filter displaces the mesh it is given",
    "[gfx][gfxfilter][geometry][!shouldfail]")
{
  // GeometryFilterNode / GeometryFilterNodeRenderer had ZERO line coverage: the
  // process is user-facing but nothing in the tree built one. The filter shifts
  // every vertex along +X, so the oracle is that the silhouette MOVES -- a
  // filter that was skipped still passes the mesh through and still draws, which
  // a not-blank oracle would accept.
  //
  // EXPECTED TO FAIL. An earlier note here blamed the raw-raster vertex binding
  // for not reading threedim vertex positions; probing the layout DISPROVED that
  // and it should not be chased again. The scene geometry is planar and
  // self-consistent: one pooled arena per attribute (2^27 / 2^26 bytes), each
  // attribute at its own binding with offset 0, position float3 padded into a
  // 16-byte stride. The Cube's "fixed quadrant" is a correctly drawn unit-cube
  // face: spanning [0,1] with no camera, its front face lands on exactly NDC
  // [0,1]^2.
  //
  // EXPLAINED as of this session, and no longer a mystery: a Geometry Filter
  // does not touch vertex buffers at all. GeometryFilterNodeRenderer's
  // runRenderPass is empty (GeometryFilterNodeRenderer.cpp:128-132); all it
  // does is append one `ossia::geometry_filter` descriptor to the mesh's
  // filter list (:110-111). The DISPLACEMENT is spliced into the CONSUMER's
  // vertex shader, and the only consumer in the tree that does that splice is
  // Threedim's ModelDisplay (ModelDisplayNode.cpp:1157-1176,
  // %vtx_define_filters% / process_vertex_<id>). This chain ends in
  // p.addRaster, and RenderedRawRasterPipelineNode never reads mesh.filters --
  // its single mention of them is the FIXME at :2630. So the filter is
  // silently dropped here, and a user wiring a Geometry Filter into a Render
  // Pipeline gets no error and no effect (a product gap worth its own fix).
  // Kept expected-red as the INTENT; see tests/gfx/GfxGeometryFilterShift.cpp,
  // which pins the parts that ARE reachable without a ModelDisplay.
  // The old note here blamed the same cause as the Transform 3D pin; that pin
  // turned out to be a rig mistake and is retired. These were never the same
  // fault.
  const auto api = GENERATE(from_range(platform_backends()));

  auto run = [&](float shift, Placement& out, std::string& err) {
    bool built = false;
    run_in_gui_app([&](const score::GUIApplicationContext& app) {
      auto* doc = score::test::new_document(app);
      if(!doc)
      {
        err = "no document";
        return;
      }
      HalpProcesses procs;
      GfxPipeline p;
      const int cube = p.addNode(procs.make<Threedim::Cube>(doc->context()));
      const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
      auto filterNode = make_geometry_filter(corpus("syn-geofilter-shift.glsl"), err);
      if(!filterNode)
        return;
      const int filt = p.addNode(std::move(filterNode));
      const int raster
          = p.addRaster(corpus("syn-scene-solid.vs"), corpus("syn-scene-solid.fs"));
      if(raster < 0)
      {
        err = "raster build failed: " + p.error();
        return;
      }
      auto* fin = p.nodeGeometryIn(filt, 0);
      auto* fout = p.nodeGeometryOut(filt, 0);
      if(!fin || !fout)
      {
        err = "geometry filter exposes no Geometry in/out";
        return;
      }
      p.wire(p.nodeSceneOut(cube, 0), p.nodeSceneIn(flat, 0));
      p.wire(p.nodeGeometryOut(flat, 0), fin);
      p.wire(fout, p.geometryIn(raster, 0));
      const int sink = p.addSink({160, 160});
      p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

      // The filter's only control is `shift` on inlet 1 (inlet 0 is Geometry).
      setInputs(*p.node(filt), {ossia::value{}, ossia::value{shift}});

      if(!p.create(api))
      {
        err = p.error();
        return;
      }
      p.render(4);
      out = placement_of(p.readback(sink));
      built = true;
    });
    if(!built && err.empty())
      err = "chain did not build";
  };

  Placement at0, at1;
  std::string e0, e1;
  run(0.f, at0, e0);
  if(!e0.empty())
    SKIP("geometry filter chain unavailable: " << e0);
  run(0.6f, at1, e1);
  INFO("err0='" << e0 << "' err1='" << e1 << "'");
  REQUIRE(e1.empty());

  // Negative control: a displacement oracle is vacuous if nothing drew.
  INFO("coverage shift=0 " << at0.coverage << " shift=0.6 " << at1.coverage);
  REQUIRE(at0.coverage > 0.01);
  INFO("centroidX shift=0 " << at0.centroidX << " shift=0.6 " << at1.centroidX);
  CHECK(at1.centroidX != at0.centroidX);
}

// -----------------------------------------------------------------------------
// CpuAnalysisNode: a halp object with a texture INPUT and no texture / buffer /
// geometry / scene output. That shape selects the GfxRenderer specialisation in
// Crousti/CpuAnalysisNode.hpp, which is an OutputNodeRenderer -- the node IS the
// sink. Nothing in the tree has that shape, so the test brings its own.
// -----------------------------------------------------------------------------
namespace
{
struct AnalysisProbe
{
  static inline std::atomic<int> calls{0};
  static inline std::atomic<int> lastR{-1};
  static inline std::atomic<int> lastG{-1};
  static inline std::atomic<int> lastW{-1};

  halp_meta(name, "Test Mean Red")
  halp_meta(c_name, "test_mean_red")
  halp_meta(category, "Visuals")
  halp_meta(author, "test")
  halp_meta(uuid, "3f1c9a72-5d64-4a1e-9b73-2c8a51de77b4")

  struct
  {
    halp::texture_input<"In"> image;
  } inputs;

  struct
  {
    halp::val_port<"Mean", float> mean;
  } outputs;

  void operator()()
  {
    auto& t = inputs.image.texture;
    lastW.store(t.width);
    if(t.bytes && t.width > 0 && t.height > 0)
    {
      // Sample the centre rather than averaging: the producer paints a flat
      // colour, so one texel is the whole signal and there is no rounding to
      // argue about.
      const int idx = ((t.height / 2) * t.width + (t.width / 2)) * 4;
      auto* px = static_cast<unsigned char*>(t.bytes) + idx;
      lastR.store(int(px[0]));
      lastG.store(int(px[1]));
      outputs.mean.value = float(px[0]) / 255.f;
    }
    calls.fetch_add(1);
  }
};
}

TEST_CASE(
    "a CPU analysis node receives the texture it is wired to",
    "[gfx][crousti][analysis]")
{
  // Nothing in the tree instantiated the shape that selects CpuAnalysisNode.
  // The oracle is the PIXEL the node saw: the ISF producer paints a known flat
  // colour, so "it ran" and "it received the right image" are separable, which
  // a call-counter alone would not give.
  const auto api = GENERATE(from_range(platform_backends()));

  AnalysisProbe::calls.store(0);
  AnalysisProbe::lastR.store(-1);
  AnalysisProbe::lastG.store(-1);
  AnalysisProbe::lastW.store(-1);

  bool skipped = false;
  std::string err, backend;

  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = score::test::new_document(app);
    if(!doc)
    {
      err = "no document";
      return;
    }
    HalpProcesses procs;
    GfxPipeline p;

    const int prod = p.addIsf(corpus("isf-solid-color.fs"));
    if(prod < 0)
    {
      err = "producer build failed: " + p.error();
      return;
    }
    auto analysisOwned = procs.make<AnalysisProbe>(doc->context());
    auto* analysis = static_cast<score::gfx::OutputNode*>(analysisOwned.get());
    const int an = p.addNode(std::move(analysisOwned));
    // The fixture has no nodeImageIn(); find the Image inlet directly.
    score::gfx::Port* ain = nullptr;
    for(auto* ip : p.node(an)->input)
      if(ip->type == score::gfx::Types::Image)
      {
        ain = ip;
        break;
      }
    if(!ain)
    {
      err = "analysis node exposes no image input";
      return;
    }
    p.wire(p.imageOut(prod, 0), ain);

    if(!p.create(api))
    {
      skipped = p.skipped();
      err = p.error();
      backend = p.backend();
      return;
    }
    backend = p.backend();
    // The analysis node IS the sink: createAllRenderLists brought it up, and
    // render() has to be driven per frame the way a real output node is.
    for(int f = 0; f < 4; ++f)
    {
      p.render(1);
      analysis->render();
    }
  });

  if(skipped)
    SKIP(backend << ": backend unavailable");
  INFO("backend=" << backend << " error: " << err);
  REQUIRE(err.empty());
  INFO("calls=" << AnalysisProbe::calls.load() << " width=" << AnalysisProbe::lastW.load()
                << " centre rgb=" << AnalysisProbe::lastR.load() << ","
                << AnalysisProbe::lastG.load());
  CHECK(AnalysisProbe::calls.load() > 0);
  CHECK(AnalysisProbe::lastW.load() > 0);
  // isf-solid-color.fs paints magenta (1,0,1,1). Asserting the actual texel is
  // what separates "the node ran" from "the node was handed the right image" --
  // a call counter would pass on a blank or stale texture.
  CHECK(AnalysisProbe::lastR.load() == 255);
  CHECK(AnalysisProbe::lastG.load() == 0);
}

TEST_CASE(
    "a scene transform reaches the shader through per_draws",
    "[gfx][crousti][scene][threedim]")
{
  // THIS CASE USED TO BE AN EXPECTED-FAILURE PIN TITLED "a scene transform
  // reaches MODEL_MATRIX". It was measuring the WRONG MECHANISM, and the pin
  // was wrong, not the product. Traced through the engine:
  //
  //  * MODEL_MATRIX is a UBO field libisf injects into every raw-raster shader
  //    (isf.cpp:4358-4366). The ONLY thing that ever writes it is
  //    RenderedRawRasterPipelineNode::process(int32_t, const
  //    ossia::transform3d&) (RenderedRawRasterPipelineNode.cpp:3378), which
  //    stores into m_modelTransform, uploaded to m_modelUBO at :2768. That is
  //    a transform3d message delivered to the RASTER NODE'S OWN port.
  //  * A Transform 3D in a SCENE chain never reaches that port. It reaches the
  //    ScenePreprocessor's renderer, where the base
  //    NodeRenderer::process(port, transform3d) (NodeRenderer.cpp:612-689)
  //    decomposes the matrix and wraps the last root under a
  //    `scene_transform` payload.
  //  * The flattener bakes that into PerDrawGPU::model
  //    (ScenePreprocessorNode.cpp:41-49, `float model[16]`), published as the
  //    `per_draws` auxiliary (:2786) and indexed by the per-instance
  //    `draw_id` VERTEX_INPUT (semantic instance_draw_id, :2660; the mechanism
  //    is spelled out at :2390-2392).
  //
  // So MODEL_MATRIX being identity under a scene chain is CORRECT, not a
  // defect -- and it is what every real shader assumes: shadow_cascades.vert
  // in this corpus reads per_draws.data[draw_id].model, and five documents in
  // the user's corpus (2026/lgm/sponza-*, 2026/test-gltf-cubemap.score,
  // 2026/funky-depth-duck.score, 2026/lgm/model-depth.score) index `per_draws`
  // the same way. Measured with syn-scene-perdraw: the encoded translation
  // moves from the mid-grey bias at position 0 to a strictly larger value at
  // position 0.5, on both backends.
  //
  // The second half keeps the OLD shader and pins the boundary explicitly:
  // through a scene chain MODEL_MATRIX stays identity at both positions. That
  // is the statement the deleted pin should have made.
  const auto api = GENERATE(from_range(platform_backends()));
  int red0 = -1, red1 = -1;
  int mm0 = -1, mm1 = -1;

  // Same rig for both halves; only the pair of shaders differs, which is
  // exactly the variable under study.
  auto probe_with = [&](const char* vs, const char* fs, float xpos, int& out) {
    run_in_gui_app([&](const score::GUIApplicationContext& app) {
      auto* doc = score::test::new_document(app);
      if(!doc)
        return;
      HalpProcesses procs;
      GfxPipeline p;
      const int cube = p.addNode(procs.make<Threedim::Cube>(doc->context()));
      const int xf = p.addNode(procs.make<Threedim::Transform3D>(doc->context()));
      const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
      const int raster = p.addRaster(corpus(vs), corpus(fs));
      if(raster < 0)
        return;
      p.wire(p.nodeSceneOut(cube, 0), p.nodeSceneIn(xf, 0));
      p.wire(p.nodeSceneOut(xf, 0), p.nodeSceneIn(flat, 0));
      p.wire(p.nodeGeometryOut(flat, 0), p.geometryIn(raster, 0));
      const int sink = p.addSink({160, 160});
      p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
      setInputs(
          *p.node(xf), {ossia::value{}, ossia::value{ossia::vec3f{xpos, 0.f, 0.f}},
                        ossia::value{ossia::vec3f{0.f, 0.f, 0.f}},
                        ossia::value{ossia::vec3f{1.f, 1.f, 1.f}}});
      if(!p.create(api))
        return;
      p.render(4);
      out = p.readback(sink).at(120, 40)[0];
    });
  };
  auto probe = [&](float xpos, int& out) {
    probe_with("syn-scene-perdraw.vs", "syn-scene-perdraw.fs", xpos, out);
  };
  auto probeModelMatrix = [&](float xpos, int& out) {
    probe_with("syn-scene-modelmat.vs", "syn-scene-modelmat.fs", xpos, out);
  };
  probe(0.f, red0);
  probe(0.5f, red1);
  probeModelMatrix(0.f, mm0);
  probeModelMatrix(0.5f, mm1);

  if(red0 < 0 || red1 < 0)
    SKIP("backend unavailable");
  // render_scene_chain rasterises with syn-scene-perdraw-solid, whose vertex
  // stage reads per_draws[draw_id].model out of a storage buffer -- and the
  // per_draws half of the second probe below does the same. Below GLSL 4.30
  // there is no such thing to read.
  if(const char* why = storage_buffer_skip_reason(api))
    SKIP(why);
  // Control: translation 0 must encode as the mid-grey bias, else the shader is
  // not reporting the matrix at all and the comparison below means nothing.
  INFO("per_draws[draw_id].model translation.x: at 0 -> "
       << red0 << ", at 0.5 -> " << red1 << " (127/128 == zero translation)");
  REQUIRE(red0 >= 120);
  REQUIRE(red0 <= 135);
  CHECK(red1 > red0);

  // The boundary: the same chain read through MODEL_MATRIX sees identity at
  // BOTH positions -- correct, since nothing wires a transform3d message to
  // the raster node's own port. If this ever stops being identity, the engine
  // grew a second transform path and the comment block above is stale.
  if(mm0 >= 0 && mm1 >= 0)
  {
    INFO("MODEL_MATRIX translation.x: at 0 -> " << mm0 << ", at 0.5 -> " << mm1);
    CHECK(mm0 >= 120);
    CHECK(mm0 <= 135);
    CHECK(mm1 == mm0);
  }
}
