// The testers that prescribe their own graph, driven by that graph.
//
// 35 of the 85 .fs testers carry an explicit `Wire:` clause in their JSON
// DESCRIPTION naming the producer chain they were authored against -- e.g.
// scene-cubemap-multiview.fs asks for "FBX scene -> SceneFlattener (per-mesh) ->
// this; CameraArrayBuilder (cubemap) -> this.cameras port". The library sweeps
// cannot honour that: ShaderSweepRaster wires one generic producer into every
// rasterizer and ShaderSweepISF wires nothing into every ISF, so a tester
// needing a camera UBO, a layered texture or a second stage renders its
// zero-input fallback and the sweep scores the silence.
//
// This file closes the gap for the subset whose prescribed graph is buildable
// out of score::gfx nodes. The upstream types the clauses name are all real
// score processes, but they are avnd/halp processes in score_plugin_threedim
// reached through the Crousti GfxNode wrapper and the document layer, while the
// gfx fixture builds score::gfx nodes directly; see the per-group notes for what
// each substitutes. score::gfx::ScenePreprocessorNode is exported and
// default-constructible, but consumes an ossia::scene_spec only a threedim
// producer can mint, so the 14 scene testers stay out of reach.
//
// Two assertions here are not "is it blank": a fullscreen tester whose output is
// a single colour by construction is uniform on every frame regardless, so those
// cases assert the frame CHANGES -- across a short and a long run for the
// persistent ones, and between the wired and unwired graph for the buffer ones.

#include "WiredCases.hpp"

using namespace score::test::gfx;
using namespace score::test::wired;

namespace
{
// -----------------------------------------------------------------------------
// Graph builders. Each mirrors one shape of `Wire:` clause.
// -----------------------------------------------------------------------------

//! `producer -> consumer.<image 0> -> sink`, where the producer is a raster
//! pipeline fed by a CSF geometry stage and the consumer a fullscreen ISF.
//! Covers the "raster tester -> inspector ISF -> Window" clauses (multiview,
//! depth-only), which no sweep can build because they span two shader kinds.
IsfResult renderRasterIntoIsf(
    score::gfx::GraphicsApi api, const QString& geoCs, const QString& rasterVs,
    const QString& rasterFs, const QString& consumerFs, QSize size = {320, 240},
    int frames = 4)
{
  IsfResult r;
  r.backend = backend_name(api);

  GfxPipeline p;
  const int geo = p.addIsf(geoCs);
  if(geo < 0)
  {
    r.error = "geometry producer build failed: " + p.error();
    return r;
  }
  const int raster = p.addRaster(rasterVs, rasterFs);
  if(raster < 0)
  {
    r.error = "raster node build failed: " + p.error();
    return r;
  }
  const int cons = p.addIsf(consumerFs);
  if(cons < 0)
  {
    r.error = "consumer build failed: " + p.error();
    return r;
  }

  auto* gout = p.geometryOut(geo, 0);
  auto* gin = p.geometryIn(raster, 0);
  if(!gout || !gin)
  {
    r.error = "no Geometry port to wire the producer to the rasterizer";
    return r;
  }
  p.wire(gout, gin);

  auto* rout = p.imageOut(raster, 0);
  auto* cin = p.imageIn(cons, 0);
  if(!rout)
  {
    r.error = "raster node exposes no Image output port";
    return r;
  }
  if(!cin)
  {
    r.error = "consumer exposes no Image input port";
    return r;
  }
  p.wire(rout, cin);

  const int s = p.addSink(size);
  auto* cout = p.imageOut(cons, 0);
  if(!cout)
  {
    r.error = "consumer exposes no Image output port";
    return r;
  }
  p.wire(cout, p.sinkInput(s));

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

//! `[buffer producer ->] consumer.<buffer 0> -> sink`. The prescribed producer
//! is CameraUBOBuilder; the producer used is the corpus' own csf-storage-rw.cs,
//! whose read_write `storage` RESOURCE exposes exactly the Types::Buffer output
//! port a uniform_input consumes.
//!
//! Not a shortcut around the real builder: ShaderSweepScene.cpp wires an actual
//! oscr::GfxNode<Threedim::Camera> into this shader's `camera` port and measures
//! the frame as byte-identical with and without it, on both backends.
//! Threedim::Camera's only outlet is an ossia::scene_spec, bindUpstreamBuffers
//! borrows solely from a Types::Buffer source, and the engine's one camera-UBO
//! delivery path is the flattener's name-matched `camera` auxiliary, which only
//! RenderedRawRasterPipelineNode consults. So for a fullscreen ISF no wiring
//! carries camera matrices, and these cases validate the binding path the
//! testers describe -- a UBO sourced from an upstream Buffer port -- not the
//! matrix contents.
IsfResult renderWithBufferProducer(
    score::gfx::GraphicsApi api, const QString& producerCs, const QString& consumerFs,
    bool connectBuffer, QSize size = {320, 240}, int frames = 4)
{
  IsfResult r;
  r.backend = backend_name(api);

  GfxPipeline p;
  int prod = -1;
  if(connectBuffer)
  {
    prod = p.addIsf(producerCs);
    if(prod < 0)
    {
      r.error = "buffer producer build failed: " + p.error();
      return r;
    }
  }
  const int cons = p.addIsf(consumerFs);
  if(cons < 0)
  {
    r.error = "consumer build failed: " + p.error();
    return r;
  }

  if(connectBuffer)
  {
    auto* bout = p.bufferOut(prod, 0);
    auto* bin = p.bufferIn(cons, 0);
    if(!bout)
    {
      r.error = "producer exposes no Buffer output port";
      return r;
    }
    if(!bin)
    {
      r.error = "consumer exposes no Buffer input port";
      return r;
    }
    p.wire(bout, bin);
  }

  auto* out = p.imageOut(cons, 0);
  if(!out)
  {
    r.error = "consumer exposes no Image output port";
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

//! `<buffer producer> -> raster.<buffer 0>` and `<geometry producer> ->
//! raster.geometry`: two separate upstreams, which is the shape
//! binding-storage-vertex.fs asks for ("any node emitting a Buffer ->
//! this.offsets -> any geometry -> this"). They must stay separate: the geometry
//! this rasterizer draws has to be the same with and without the SSBO edge, or
//! the comparison measures the producer swap instead of the binding.
IsfResult renderRasterWithBuffer(
    score::gfx::GraphicsApi api, const QString& geoCs, const QString& bufferCs,
    const QString& rasterVs, const QString& rasterFs, bool connectBuffer,
    QSize size = {320, 240}, int frames = 4)
{
  IsfResult r;
  r.backend = backend_name(api);

  GfxPipeline p;
  const int geo = p.addIsf(geoCs);
  if(geo < 0)
  {
    r.error = "geometry producer build failed: " + p.error();
    return r;
  }
  const int raster = p.addRaster(rasterVs, rasterFs);
  if(raster < 0)
  {
    r.error = "raster node build failed: " + p.error();
    return r;
  }

  auto* gout = p.geometryOut(geo, 0);
  auto* gin = p.geometryIn(raster, 0);
  if(!gout || !gin)
  {
    r.error = "no Geometry port to wire the producer to the rasterizer";
    return r;
  }
  p.wire(gout, gin);

  if(connectBuffer)
  {
    const int buf = p.addIsf(bufferCs);
    if(buf < 0)
    {
      r.error = "buffer producer build failed: " + p.error();
      return r;
    }
    auto* bout = p.bufferOut(buf, 0);
    auto* bin = p.bufferIn(raster, 0);
    if(!bout)
    {
      r.error = "producer exposes no Buffer output port";
      return r;
    }
    if(!bin)
    {
      r.error = "rasterizer exposes no Buffer input port";
      return r;
    }
    p.wire(bout, bin);
  }

  auto* out = p.imageOut(raster, 0);
  if(!out)
  {
    r.error = "raster node exposes no Image output port";
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

//! `this -> sink` plus `this.out -> this.<image k>`: the self-feedback edge
//! feedback-texture.fs prescribes. WIRING.md lists this as the one shape the
//! JS builder cannot express ("delayed-edge feedback cable (Score API doesn't
//! expose it)"), so the graph fixture is the only place it can be built.
IsfResult renderSelfFeedback(
    score::gfx::GraphicsApi api, const QString& fs, bool feedback,
    QSize size = {320, 240}, int frames = 8)
{
  IsfResult r;
  r.backend = backend_name(api);

  GfxPipeline p;
  const int n = p.addIsf(fs);
  if(n < 0)
  {
    r.error = "node build failed: " + p.error();
    return r;
  }
  auto* out = p.imageOut(n, 0);
  if(!out)
  {
    r.error = "node exposes no Image output port";
    return r;
  }
  const int s = p.addSink(size);
  p.wire(out, p.sinkInput(s));

  if(feedback)
  {
    auto* in = p.imageIn(n, 0);
    if(!in)
    {
      r.error = "node exposes no Image input port to feed back into";
      return r;
    }
    p.wireFeedback(out, in);
  }

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

// -----------------------------------------------------------------------------
// The cases.
// -----------------------------------------------------------------------------

//! Wire: `output-format-rgba16f -> isf-image-passthrough -> Window`, and
//! `output-layered -> binding-image-array -> Window`. Both are ISF-to-ISF, which
//! render_isf_chain already builds; the sweep just never chains anything.
void isfChainCases(score::gfx::GraphicsApi api, const QString& root)
{
  const auto path = [&](const char* rel) -> QString {
    return root + QStringLiteral("/") + QString::fromUtf8(rel);
  };

  {
    announce("output-format-rgba16f -> isf-image-passthrough");
    Outcome o;
    auto r = render_isf_chain(
        api, {path("tests-scene/output-format-rgba16f.fs"),
              path("isf-image-passthrough.fs")},
        {320, 240}, 4);
    if(harvest(r, o))
    {
      const auto& im = r.outputs.front();
      o.note = describe(im);
      // The passthrough draws a 2x2 grid with black gutters, so a correctly
      // sampled rgba16f source can never come back uniform.
      if(isUniformImage(im))
        o.failure = "chain output uniform: the rgba16f attachment did not reach "
                    "the passthrough sampler (" + o.note + ")";
      else if(isBlack(im))
        o.failure = "chain output all black (" + o.note + ")";
    }
    record("output-format-rgba16f -> isf-image-passthrough", std::move(o));
  }

  {
    announce("output-layered -> binding-image-array");
    Outcome o;
    auto r = render_isf_chain(
        api,
        {path("tests-scene/output-layered.fs"),
         path("tests-scene/binding-image-array.fs")},
        {320, 240}, 4);
    if(harvest(r, o))
    {
      const auto& im = r.outputs.front();
      o.note = describe(im);
      // The consumer samples one whole layer, so uniform is expected — what
      // must not happen is black, which is "the array binding produced nothing".
      if(isBlack(im))
        o.failure = "sampler2DArray consumer read black: the 4-layer render "
                    "target did not bind (" + o.note + ")";
    }
    record("output-layered -> binding-image-array", std::move(o));
  }
}

//! Wire: `<geometry> -> raster tester -> inspector ISF -> Window`.
void rasterIntoIsfCases(score::gfx::GraphicsApi api, const QString& root)
{
  const auto path = [&](const char* rel) -> QString {
    return root + QStringLiteral("/") + QString::fromUtf8(rel);
  };
  const QString geo = path("csf-vertex-count-expr.cs");

  {
    announce("output-multiview -> binding-image-array");
    Outcome o;
    auto r = renderRasterIntoIsf(
        api, geo, path("tests-scene/output-multiview.vs"),
        path("tests-scene/output-multiview.fs"),
        path("tests-scene/binding-image-array.fs"));
    if(harvest(r, o))
    {
      const auto& im = r.outputs.front();
      o.note = describe(im);
      if(isBlack(im))
        o.failure = "multiview layers never reached the array sampler ("
                    + o.note + ")";
    }
    record("output-multiview -> binding-image-array", std::move(o));
  }

  {
    announce("output-depth-only -> isf-image-depth");
    Outcome o;
    auto r = renderRasterIntoIsf(
        api, geo, path("tests-scene/output-depth-only.vs"),
        path("tests-scene/output-depth-only.fs"), path("isf-image-depth.fs"));
    if(harvest(r, o))
    {
      const auto& im = r.outputs.front();
      o.note = describe(im);
      if(isBlack(im))
        o.failure = "depth-only attachment read back black in the inspector ("
                    + o.note + ")";
    }
    record("output-depth-only -> isf-image-depth", std::move(o));
  }
}

//! Wire: `<Buffer producer> -> this.<uniform_input> -> Window`. Asserted as a
//! DIFFERENCE against the same graph with the edge absent: every one of these
//! shaders is fullscreen and single-coloured, so "not uniform" is meaningless
//! and "not black" passes on the zero-filled placeholder UBO the renderer binds
//! when nothing upstream is connected.
void uniformInputCases(score::gfx::GraphicsApi api, const QString& root)
{
  const auto path = [&](const QString& rel) -> QString { return root + QStringLiteral("/") + rel; };
  const QString producer = path(QStringLiteral("csf-storage-rw.cs"));

  const QString consumers[] = {
      QStringLiteral("tests-scene/binding-uniform-input.fs"),
      QStringLiteral("tests-scene/scene-camera-ubo.fs"),
      QStringLiteral("tests-scene/scene-camera-array.fs"),
      QStringLiteral("isf-mrt-uniform-input.fs"),
      QStringLiteral("isf-persistent-uniform-input.fs"),
      // Last of the list, and the list is last of the run: this one SIGSEGVs
      // rather than failing (see runWired).
      QStringLiteral("isf-multipass-uniform-input.fs"),
  };

  for(const auto& rel : consumers)
  {
    announce(rel.toStdString());
    Outcome o;
    auto wired = renderWithBufferProducer(api, producer, path(rel), true);
    if(!harvest(wired, o))
    {
      record(rel.toStdString(), std::move(o));
      continue;
    }
    auto bare = renderWithBufferProducer(api, producer, path(rel), false);
    Outcome bo;
    if(!harvest(bare, bo))
    {
      o.failure = "unwired reference render failed: " + bo.failure;
      record(rel.toStdString(), std::move(o));
      continue;
    }

    o.note = "wired " + describe(wired.outputs.front()) + " / unwired "
             + describe(bare.outputs.front());
    if(sameImage(wired.outputs.front(), bare.outputs.front()))
      o.failure = "upstream Buffer had no effect: the uniform_input binding did "
                  "not reach the pass SRB (" + o.note + ")";
    record(rel.toStdString(), std::move(o));
  }
}

//! Wire: `<Buffer> -> this.offsets -> <geometry> -> this -> Window`, and the
//! MDI-shaped `<geometry + indirect cmds> -> this -> Window`.
void rasterBufferCases(score::gfx::GraphicsApi api, const QString& root)
{
  const auto path = [&](const char* rel) -> QString {
    return root + QStringLiteral("/") + QString::fromUtf8(rel);
  };

  {
    // The tester's DESCRIPTION says "the vertex shader reads element 0 as an
    // offset", but binding-storage-vertex.vs never mentions `offsets` — it is a
    // plain `gl_Position = clipSpaceCorrMatrix * vec4(position, 1.0)`. No wiring
    // can therefore make the SSBO move the geometry, so asserting a visible
    // difference would be asserting against the corpus. What the wiring CAN
    // establish is the half the shader header does describe: a vertex-visible
    // SSBO binding must not disturb the draw or trip validation.
    announce("binding-storage-vertex + upstream Buffer");
    Outcome o;
    auto wired = renderRasterWithBuffer(
        api, path("csf-vertex-count-expr.cs"), path("csf-storage-rw.cs"),
        path("tests-scene/binding-storage-vertex.vs"),
        path("tests-scene/binding-storage-vertex.fs"), true);
    if(harvest(wired, o))
    {
      const auto& im = wired.outputs.front();
      o.note = describe(im);
      if(isUniformImage(im))
        o.failure = "nothing drawn once a vertex-stage SSBO is bound (" + o.note
                    + ")";
    }
    record("binding-storage-vertex + upstream Buffer", std::move(o));
  }

  {
    // The prescribed producer is SceneFlattener in MDI mode (outlet 1 = indirect
    // cmds). csf-indirect-draw.cs is the corpus' own GPU-generated MDI producer:
    // it emits two triangles AND writes the indirect command buffer that rides
    // with the geometry, so the geometry edge alone drives the indirect path.
    // The tester's separate `indirectCmds` storage port stays unwired — nothing
    // reachable here publishes a standalone indirect Buffer.
    announce("binding-indirect-draw <- csf-indirect-draw");
    Outcome o;
    auto r = render_raster(
        api, {path("csf-indirect-draw.cs")},
        path("tests-scene/binding-indirect-draw.vs"),
        path("tests-scene/binding-indirect-draw.fs"), {320, 240}, 4);
    if(harvest(r, o))
    {
      const auto& im = r.outputs.front();
      o.note = describe(im);
      if(isUniformImage(im))
        o.failure = "no geometry drawn from the indirect producer (" + o.note + ")";
    }
    record("binding-indirect-draw <- csf-indirect-draw", std::move(o));
  }
}

//! Wire: `this -> Window` — already what the sweep builds. What the sweep gets
//! wrong is the question: these shaders paint a single colour derived from a
//! frame counter, so they are uniform by construction and the uniformity check
//! calls every one of them blank. The property that actually distinguishes a
//! working ping-pong from a broken one is that the colour ADVANCES.
void persistentCases(score::gfx::GraphicsApi api, const QString& root)
{
  const auto path = [&](const QString& rel) -> QString { return root + QStringLiteral("/") + rel; };

  // isf-multipass-storage-rw.fs belongs to this family by construction but not
  // by clock: its SSBO carries TIME, and pump_frame feeds `date` in flicks, so
  // ISF TIME stays ~0 for every frame of every run (the same limitation
  // IsfUniforms.cpp works around by keying on FRAMEINDEX). Its red channel is
  // pinned at 0 regardless of whether the same-frame write→read works, so it
  // gets the weaker "pass 1 rendered the pattern at all" assertion below.
  const QString shaders[] = {
      QStringLiteral("isf-mrt-persistent-ssbo.fs"),
      QStringLiteral("isf-multipass-persistent-ssbo.fs"),
      QStringLiteral("tests-scene/binding-storage-persistent.fs"),
  };

  for(const auto& rel : shaders)
  {
    announce(rel.toStdString());
    Outcome o;
    auto shortRun = render_isf_chain(api, {path(rel)}, {320, 240}, 2);
    if(!harvest(shortRun, o))
    {
      record(rel.toStdString(), std::move(o));
      continue;
    }
    auto longRun = render_isf_chain(api, {path(rel)}, {320, 240}, 24);
    Outcome lo;
    if(!harvest(longRun, lo))
    {
      o.failure = "long run failed: " + lo.failure;
      record(rel.toStdString(), std::move(o));
      continue;
    }

    o.note = "2 frames " + describe(shortRun.outputs.front()) + " / 24 frames "
             + describe(longRun.outputs.front());
    if(sameImage(shortRun.outputs.front(), longRun.outputs.front()))
      o.failure = "counter did not advance across the run: the persistent SSBO "
                  "never ping-ponged (" + o.note + ")";
    record(rel.toStdString(), std::move(o));
  }

  {
    announce("isf-multipass-storage-rw.fs");
    Outcome o;
    auto r = render_isf_chain(
        api, {path(QStringLiteral("isf-multipass-storage-rw.fs"))}, {320, 240}, 4);
    if(harvest(r, o))
    {
      const auto& im = r.outputs.front();
      o.note = describe(im);
      if(isUniformImage(im))
        o.failure = "pass 1 drew nothing: the SSBO binding did not reach the "
                    "second pass' SRB (" + o.note + ")";
    }
    record("isf-multipass-storage-rw.fs", std::move(o));
  }
}

//! Wire: `this -> Window` + `this.out -> this.prev` (Delayed).
void feedbackCase(score::gfx::GraphicsApi api, const QString& root)
{
  const auto path = [&](const char* rel) -> QString {
    return root + QStringLiteral("/") + QString::fromUtf8(rel);
  };

  announce("feedback-texture self-edge");
  Outcome o;
  auto fed = renderSelfFeedback(api, path("feedback-texture.fs"), true);
  auto bare = renderSelfFeedback(api, path("feedback-texture.fs"), false);
  Outcome bo;
  const bool bareOk = harvest(bare, bo);

  if(!harvest(fed, o))
  {
    // Say plainly whether the same node renders WITHOUT the self-edge: that is
    // what separates "the cycle killed the render list" from "this shader never
    // renders here".
    o.failure += bareOk ? " (the same node without the self-edge renders "
                          + describe(bare.outputs.front()) + ")"
                        : " (and the no-feedback variant fails too: " + bo.failure
                              + ")";
  }
  else if(!bareOk)
  {
    o.failure = "no-feedback reference render failed: " + bo.failure;
  }
  else
  {
    o.note = "feedback " + describe(fed.outputs.front()) + " / none "
             + describe(bare.outputs.front());
    if(sameImage(fed.outputs.front(), bare.outputs.front()))
      o.failure = "the self-edge produced no trail: the feedback texture was "
                  "never sampled (" + o.note + ")";
  }
  record("feedback-texture self-edge", std::move(o));
}

void runWired(const score::GUIApplicationContext& ctx)
{
  const QString root = libraryRoot(ctx);
  if(root.isEmpty() || !QFileInfo::exists(root))
    SKIP("no shader library available (set SCORE_SHADER_LIBRARY_DIR)");
  if(!QFileInfo::exists(root + QStringLiteral("/tests-scene")))
    SKIP("library has no tests-scene/ directory: these fixtures drive the "
         "csf-testers corpus, point SCORE_SHADER_LIBRARY_DIR at it");

  const auto* gfx_settings = ctx.findSettings<Gfx::Settings::Model>();
  if(!gfx_settings)
    FAIL("score_plugin_gfx registered no settings model: run from the build root.");
  const auto api = gfx_settings->graphicsApiEnum();

  isfChainCases(api, root);
  rasterIntoIsfCases(api, root);
  rasterBufferCases(api, root);
  persistentCases(api, root);
  // A self-edge is a cycle, and a renderer that cannot break it hangs rather
  // than fails.
  feedbackCase(api, root);
  // Last, because it does not fail — it SIGSEGVs. isf-multipass-uniform-input
  // takes the process down in RenderedISFNode::runRenderPass's
  // cb.setShaderResources(srb) as soon as an upstream Buffer is wired into its
  // uniform_input, so anything ordered after it would never be measured. Every
  // case above has been recorded by the time we get here.
  uniformInputCases(api, root);
}
}

TEST_CASE(
    "Testers with a prescribed Wire: clause render through that graph",
    "[integration][gfx][shaders]")
{
  requestGlesContext();
  results().clear();
  score::test::run_in_gui_app(&runWired);
  assertAll();
}
