// The scene testers, driven by the real SceneFlattener / CameraUBOBuilder /
// DirectionalLight / Primitive-cube chain their `Wire:` clause names.
//
// ShaderSweepWired.cpp covers the 21 clauses buildable out of score::gfx nodes
// alone and documents why the other 14 were not: every upstream type the clauses
// name (SceneFlattener, CameraUBOBuilder, CameraArrayBuilder, DirectionalLight,
// FbxLoader, ObjLoader, Primitive cube, BufferLoader) is an avnd/halp process in
// score_plugin_threedim, reached through the Crousti wrapper and the document
// layer, while the gfx fixture builds score::gfx nodes directly.
//
// That gap is closable without a running execution engine. oscr::GfxNode<T> —
// the Crousti wrapper that turns a halp process into a score::gfx node — needs
// only two things from the document layer:
//
//     GfxNode(oscr::ProcessModel<T>& element,
//             std::weak_ptr<Execution::ExecutionCommandQueue> q,   // may be empty
//             Gfx::exec_controls ctls,                             // may be empty
//             int64_t id,
//             const score::DocumentContext& ctx)
//
// a ProcessModel (constructible standalone: duration, id, ctx, parent) and a
// DocumentContext (score::test::new_document gives one). No interval, no
// executor, no transport. So this file instantiates the REAL producers and wires
// them into the REAL score::gfx::ScenePreprocessorNode — which is what
// "SceneFlattener" is, exported and default-constructible all along; what it
// lacked was an ossia::scene_spec source, and a halp scene producer is exactly
// that.
//
// One deliberate deviation from tests-scene/common.js: the JS builder wires
// CameraUBOBuilder to the CONSUMER's inlet 1, not to the flattener. A camera
// outlet is a scene/geometry port and a consumer's `camera` uniform_input is a
// Types::Buffer port, so that edge binds nothing (IsfBindingsBuilder.cpp:724
// requires a Buffer source) and the flattener silently packs its default eye at
// (0,1,3) instead. The camera reaches the shader through the flattener's
// name-matched `camera` auxiliary buffer (ScenePreprocessorNode.cpp:3216 +
// bindUpstreamBuffersFromGeometry), so here the Camera is wired INTO the
// flattener alongside the mesh and the light — additive multi-producer merge on
// one scene port is what NodeRenderer::process(port, scene_spec, source_key) is
// for.

#include "WiredCases.hpp"

#include <score_test/Document.hpp>

#include <Threedim/Camera.hpp>
#include <Threedim/Light.hpp>
#include <Threedim/Primitive.hpp>

#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <core/document/Document.hpp>

using namespace score::test::gfx;
using namespace score::test::wired;

namespace
{
//! Owns the ProcessModels the GfxNodes hold references to. Declared before the
//! GfxPipeline at every call site so it is destroyed AFTER the nodes that point
//! into it.
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
    return std::unique_ptr<score::gfx::Node>{new oscr::GfxNode<T>{
        *raw, {}, Gfx::exec_controls{}, next++, ctx}};
  }
};

struct SceneOpts
{
  bool camera = true;
  bool light = true;
  //! false wires the cube straight into the rasterizer, skipping the flattener —
  //! the shape ps-cull-mode / ps-polygon-line / ps-depth-test ask for
  //! ("Primitive cube producer -> this -> Window").
  bool flatten = true;
};

//! Builds `Primitive cube [+ Camera + DirectionalLight] -> SceneFlattener ->
//! raster tester -> sink`, or the direct `cube -> raster tester -> sink`.
IsfResult renderSceneChain(
    const score::DocumentContext& doc, score::gfx::GraphicsApi api,
    const QString& vsPath, const QString& fsPath, SceneOpts opts,
    QSize size = {320, 240}, int frames = 4)
{
  IsfResult r;
  r.backend = backend_name(api);

  HalpProcesses procs;
  GfxPipeline p;

  const int cube = p.addNode(procs.make<Threedim::Cube>(doc));
  const int raster = p.addRaster(vsPath, fsPath);
  if(raster < 0)
  {
    r.error = "raster node build failed: " + p.error();
    return r;
  }

  auto* rasterGeo = p.geometryIn(raster, 0);
  if(!rasterGeo)
  {
    r.error = "raster node has no Geometry input port";
    return r;
  }

  if(opts.flatten)
  {
    const int flat
        = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());

    auto* flatIn = p.nodeSceneIn(flat, 0);
    auto* cubeOut = p.nodeSceneOut(cube, 0);
    if(!flatIn || !cubeOut)
    {
      r.error = "no Scene port to wire the mesh producer into the flattener";
      return r;
    }
    p.wire(cubeOut, flatIn);

    if(opts.camera)
    {
      const int cam = p.addNode(procs.make<Threedim::Camera>(doc));
      if(auto* out = p.nodeSceneOut(cam, 0))
        p.wire(out, flatIn);
      else
      {
        r.error = "Camera exposes no Scene output port";
        return r;
      }
    }
    if(opts.light)
    {
      const int light = p.addNode(procs.make<Threedim::Light>(doc));
      if(auto* out = p.nodeSceneOut(light, 0))
        p.wire(out, flatIn);
      else
      {
        r.error = "DirectionalLight exposes no Scene output port";
        return r;
      }
    }

    auto* flatOut = p.nodeGeometryOut(flat, 0);
    if(!flatOut)
    {
      r.error = "flattener exposes no Geometry output port";
      return r;
    }
    p.wire(flatOut, rasterGeo);
  }
  else
  {
    auto* cubeOut = p.nodeSceneOut(cube, 0);
    if(!cubeOut)
    {
      r.error = "cube exposes no Geometry output port";
      return r;
    }
    p.wire(cubeOut, rasterGeo);
  }

  auto* out = p.imageOut(raster, 0);
  if(!out)
  {
    r.error = "raster node has no Image output port";
    return r;
  }
  const int s = p.addSink(size);
  p.wire(out, p.sinkInput(s));

  if(!p.create(api))
  {
    r.backend = p.backend();
    r.skipped = p.skipped();
    r.skip_reason = p.skipReason();
    r.error = p.error();
    return r;
  }
  r.backend = p.backend();
  p.render(frames);
  r.outputs.push_back(p.readback(s));
  return r;
}

struct SceneCase
{
  const char* rel;
  SceneOpts opts;
  //! Non-null when the tester needs scene CONTENT a Primitive cube cannot
  //! supply. The case still runs and still reports the frame it measured — the
  //! evidence is worth having — but it is recorded as a skip, because a black
  //! frame from a shader that was handed none of the data it indexes says
  //! nothing about the renderer.
  const char* unmet = nullptr;
  //! Non-null when this tester's own broken-state output is a uniformly BLACK
  //! frame (it shades by a light term that reads zero). "Not uniform" is then
  //! the wrong assertion — a correctly drawn but unlit mesh is indistinguishable
  //! from an undrawn one by pixels alone. The named differential below carries
  //! the verdict instead, so the case only records what it measured.
  const char* verdictFrom = nullptr;
};

void runScene(const score::GUIApplicationContext& ctx)
{
  const QString root = libraryRoot(ctx);
  if(root.isEmpty() || !QFileInfo::exists(root))
    SKIP("no shader library available (set SCORE_SHADER_LIBRARY_DIR)");
  if(!QFileInfo::exists(root + QStringLiteral("/tests-scene")))
    SKIP("library has no tests-scene/ directory: point SCORE_SHADER_LIBRARY_DIR "
         "at the csf-testers corpus");

  const auto* gfx_settings = ctx.findSettings<Gfx::Settings::Model>();
  if(!gfx_settings)
    FAIL("score_plugin_gfx registered no settings model: run from the build root.");
  const auto api = gfx_settings->graphicsApiEnum();

  auto* document = score::test::new_document(ctx);
  if(!document)
    FAIL("could not create a document: oscr::ProcessModel needs a DocumentContext");
  const score::DocumentContext& doc = document->context();

  // The scene chain, per the `Wire:` clauses. The prescribed mesh source is an
  // FbxLoader/ObjLoader for most of these; the Primitive cube stands in because
  // driving AssetLoader means feeding it a file through the halp raw_file_data
  // worker, which does need the execution layer. Every OTHER node in the chain
  // is the real one, and the cube exercises the same flatten path (mesh
  // primitive -> per_draws/scene_materials/scene_lights/camera aux).
  const SceneCase flattened[] = {
      {"tests-scene/scene-aux-materials", {}},
      {"tests-scene/scene-skinning-joint-matrices", {}},
      {"tests-scene/scene-aux-lights", {}, nullptr,
       "scene-aux-lights: DirectionalLight changes the frame"},
      {"tests-scene/fbx-classic-pbr", {}, nullptr,
       "scene-aux-lights: DirectionalLight changes the frame"},

      // Buildable chain, unbuildable CONTENT. Each names what is missing; none
      // of these is a renderer verdict.
      {"tests-scene/scene-aux-per-draw", {},
       "needs an FBX with 3 meshes carrying distinct material.tag values; one "
       "Primitive cube flattens to a single draw, so the per-draw tint the "
       "tester compares has nothing to vary over"},
      {"tests-scene/scene-data-forward", {},
       "needs a loader that emits scene_data{name:'noise_field'} alongside a "
       "mesh; no producer reachable here attaches a named scene_data payload"},
      {"tests-scene/scene-instance-transforms", {},
       "needs a scene containing an instance_component_ptr (prototype + N "
       "transforms); Primitive cube emits a single non-instanced mesh, so the "
       "flattener attaches no instance_transforms aux"},
      {"tests-scene/scene-mdi", {},
       "clause asks for SceneFlattener MDI outlets 1 (indirect) and 2 "
       "(per_draws) as separate edges, but ScenePreprocessorNode exposes ONE "
       "output port — every scene-wide buffer rides it as an auxiliary"},
      {"tests-scene/scene-cubemap-multiview", {},
       "needs CameraArrayBuilder in cubemap mode on .cameras; nothing in the "
       "engine publishes a 'cameras' aux or a CameraEntry[6] UBO (the "
       "flattener's camera array is published under the name 'camera')"},
      {"tests-scene/fbx-textured-pbr", {},
       "clause asks for SceneFlattener outlet 3 (base_color_array) as a "
       "separate edge; ScenePreprocessorNode exposes ONE output port"},
  };

  // "Primitive cube producer -> this -> Window": no flatten. These three are the
  // ones the generic sweep could never satisfy — ps-cull-mode needs a `normal`
  // vertex attribute and ps-depth-test needs non-zero Z, and no .cs in the
  // corpus emits either (every geometry attribute across all 40 is
  // position/color/sprite_size, all at z=0). Threedim::Cube emits
  // position + normal + texcoord with real depth.
  const SceneCase direct[] = {
      {"tests-scene/ps-cull-mode", {.flatten = false}},
      {"tests-scene/ps-polygon-line", {.flatten = false}},
      {"tests-scene/ps-depth-test", {.flatten = false}},
  };

  const auto run = [&](const SceneCase& c) {
    announce(c.rel);
    Outcome o;
    const QString base = root + QStringLiteral("/") + QString::fromUtf8(c.rel);
    auto r = renderSceneChain(
        doc, api, base + QStringLiteral(".vs"), base + QStringLiteral(".fs"),
        c.opts);
    if(harvest(r, o))
    {
      const auto& im = r.outputs.front();
      o.note = describe(im);
      if(c.unmet)
      {
        o.skipped = true;
        o.skip_reason = c.unmet;
        o.note += " — not asserted: ";
        o.note += c.unmet;
      }
      else if(c.verdictFrom)
      {
        o.skipped = true;
        o.skip_reason = std::string("verdict carried by \"") + c.verdictFrom + "\"";
        o.note += " — unlit output is black by construction; see \"";
        o.note += c.verdictFrom;
        o.note += "\"";
      }
      else if(isUniformImage(im))
      {
        o.failure = "nothing drawn (" + o.note + ")";
      }
    }
    record(c.rel, std::move(o));
  };

  for(const auto& c : flattened)
    run(c);
  for(const auto& c : direct)
    run(c);

  // The camera testers' clause is "CameraUBOBuilder -> this.camera port", and
  // ShaderSweepWired.cpp currently substitutes csf-storage-rw.cs for the
  // builder. Wire the REAL one and measure what happens: Threedim::Camera's only
  // outlet is `scene_out` (an ossia::scene_spec), stamped Types::Geometry by
  // port_to_type_enum, while a fullscreen ISF's uniform_input is a
  // Types::Buffer port — and bindUpstreamBuffers only borrows from a source of
  // type Buffer (IsfBindingsBuilder.cpp:724). If that means the edge delivers
  // nothing, the frame is unchanged by its presence, and the substitution in
  // ShaderSweepWired.cpp is not a shortcut but the only wiring that reaches the
  // binding at all.
  {
    announce("binding-uniform-input <- real CameraUBOBuilder");
    Outcome o;
    const QString consumer
        = root + QStringLiteral("/tests-scene/binding-uniform-input.fs");

    const auto renderWithCamera = [&](bool connect) {
      IsfResult r;
      r.backend = backend_name(api);
      HalpProcesses procs;
      GfxPipeline p;
      const int cons = p.addIsf(consumer);
      if(cons < 0)
      {
        r.error = "consumer build failed: " + p.error();
        return r;
      }
      if(connect)
      {
        const int cam = p.addNode(procs.make<Threedim::Camera>(doc));
        auto* camOut = p.nodeSceneOut(cam, 0);
        auto* camIn = p.bufferIn(cons, 0);
        if(!camOut)
        {
          r.error = "Camera exposes no scene output port";
          return r;
        }
        if(!camIn)
        {
          r.error = "consumer exposes no Buffer input port";
          return r;
        }
        p.wire(camOut, camIn);
      }
      auto* out = p.imageOut(cons, 0);
      const int s = p.addSink({320, 240});
      p.wire(out, p.sinkInput(s));
      if(!p.create(api))
      {
        r.backend = p.backend();
        r.skipped = p.skipped();
        r.skip_reason = p.skipReason();
        r.error = p.error();
        return r;
      }
      r.backend = p.backend();
      p.render(4);
      r.outputs.push_back(p.readback(s));
      return r;
    };

    auto wired = renderWithCamera(true);
    if(harvest(wired, o))
    {
      auto bare = renderWithCamera(false);
      Outcome bare_o;
      if(!harvest(bare, bare_o))
      {
        o.failure = "unwired reference render failed: " + bare_o.failure;
      }
      else
      {
        o.note = "with Camera " + describe(wired.outputs.front()) + " / without "
                 + describe(bare.outputs.front());
        if(sameImage(wired.outputs.front(), bare.outputs.front()))
          o.failure = "a real CameraUBOBuilder edge into a fullscreen ISF's "
                      "uniform_input delivers nothing: its outlet is a scene "
                      "port, and only a Types::Buffer source is borrowed ("
                      + o.note + ")";
      }
    }
    record("binding-uniform-input <- real CameraUBOBuilder", std::move(o));
  }

  // ISOLATION. Across the flattened testers, drawing splits cleanly on one
  // thing: the two that declare only `position` in VERTEX_INPUTS render
  // (scene-aux-materials, scene-skinning-joint-matrices, cover=0.25) and the two
  // that also declare `normal` render nothing (scene-aux-lights,
  // fbx-classic-pbr). ps-cull-mode declares `position + normal` too and draws —
  // but it is wired to the cube DIRECTLY, so the attribute is one candidate
  // explanation. Run that one shader down BOTH paths, changing nothing but
  // whether the flattener sits in the middle. It also establishes, for the light
  // differential below, whether a mesh reaches the rasterizer through the
  // flattener at all — which is the premise that turns "black" into "unlit"
  // rather than "undrawn".
  bool flattenedGeometryDraws = false;
  {
    announce("SceneFlattener preserves the `normal` vertex attribute");
    Outcome o;
    const QString base = root + QStringLiteral("/tests-scene/ps-cull-mode");
    auto direct_r = renderSceneChain(
        doc, api, base + QStringLiteral(".vs"), base + QStringLiteral(".fs"),
        SceneOpts{.flatten = false});
    auto flat_r = renderSceneChain(
        doc, api, base + QStringLiteral(".vs"), base + QStringLiteral(".fs"),
        SceneOpts{.flatten = true});
    Outcome direct_o;
    if(!harvest(direct_r, direct_o))
    {
      o.failure = "direct cube->rasterizer reference render failed: "
                  + direct_o.failure;
    }
    else if(harvest(flat_r, o))
    {
      o.note = "cube->raster " + describe(direct_r.outputs.front())
               + " / cube->flattener->raster " + describe(flat_r.outputs.front());
      flattenedGeometryDraws = !isUniformImage(flat_r.outputs.front());
      if(isUniformImage(direct_r.outputs.front()))
        o.failure = "the direct reference drew nothing either, so this cannot "
                    "isolate the flattener (" + o.note + ")";
      else if(!flattenedGeometryDraws)
        o.failure = "the SAME shader and mesh draw directly but not through "
                    "SceneFlattener: the flattened geometry does not satisfy a "
                    "`position + normal` vertex layout (" + o.note + ")";
    }
    record("SceneFlattener preserves the `normal` vertex attribute", std::move(o));
  }

  // scene-aux-lights colours by N·L of scene_lights.entries[0], and its own
  // header says a black frame means "entries array length reads 0". Pixels alone
  // cannot separate that from "the mesh never drew" — both are black — so this
  // needs two independent facts: that a mesh DOES reach the rasterizer through
  // this exact flattened chain (established just above with a shader whose
  // output does not depend on lighting), and that removing the DirectionalLight
  // changes nothing. Together those two say the light is missing from
  // scene_lights, which is a routing answer rather than a shading one.
  {
    announce("scene-aux-lights: DirectionalLight changes the frame");
    Outcome o;
    const QString base
        = root + QStringLiteral("/tests-scene/scene-aux-lights");
    auto lit = renderSceneChain(
        doc, api, base + QStringLiteral(".vs"), base + QStringLiteral(".fs"),
        SceneOpts{.light = true});
    if(harvest(lit, o))
    {
      auto dark = renderSceneChain(
          doc, api, base + QStringLiteral(".vs"), base + QStringLiteral(".fs"),
          SceneOpts{.light = false});
      Outcome dark_o;
      if(!harvest(dark, dark_o))
      {
        o.failure = "no-light reference render failed: " + dark_o.failure;
      }
      else
      {
        o.note = "with light " + describe(lit.outputs.front()) + " / without "
                 + describe(dark.outputs.front())
                 + (flattenedGeometryDraws
                        ? "; a lighting-independent shader DOES draw through "
                          "this same flattened chain"
                        : "; no mesh reaches the rasterizer through this chain "
                          "at all");
        if(!sameImage(lit.outputs.front(), dark.outputs.front()))
        {
          // Different frames: the light reached scene_lights. Nothing to say.
        }
        else if(!flattenedGeometryDraws)
        {
          o.failure = "inconclusive: nothing draws through the flattened chain, "
                      "so the black output cannot be attributed to the light ("
                      + o.note + ")";
        }
        else
        {
          o.failure = "the DirectionalLight never reached scene_lights: geometry "
                      "demonstrably draws through this chain, yet the frame is "
                      "byte-identical with and without the light (" + o.note + ")";
        }
      }
    }
    record("scene-aux-lights: DirectionalLight changes the frame", std::move(o));
  }
}
}

TEST_CASE(
    "Scene testers render through the real SceneFlattener chain",
    "[integration][gfx][shaders][scene]")
{
  requestGlesContext();
  results().clear();
  score::test::run_in_gui_app(&runScene);
  assertAll();
}
