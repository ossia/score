// =============================================================================
// MODEL_MATRIX SURVIVES THE MULTIVIEW BINDING SHIFT.
//
// isf.cpp's parse_raw_raster_pipeline() reserves a descriptor slot for the
// multiview UBO when MULTIVIEW >= 2 and then bumps model_ubo_binding past it:
//
//     if(m_desc.multiview_count >= 2) {
//       material_ubos += isf_emit_multiview_ubo(sampler_binding, ...);
//       sampler_binding++;
//     }
//     int model_ubo_binding = sampler_binding;
//
// so a MULTIVIEW shader declares model_material_t one slot higher than the
// same shader without MULTIVIEW. RenderedRawRasterPipelineNode counts its own
// bindings independently -- `int max_binding = 3` plus samplers plus auxiliary
// textures -- and then pushes m_modelUBO at max_binding. Nothing in that file
// reserves the multiview slot (RenderedISFNode and SimpleRenderedISFNode both
// create an m_multiViewUBO; the raw-raster renderer has none), so the two
// numbers disagree by exactly one as soon as MULTIVIEW >= 2.
//
// Nothing caught this because no existing multiview raw-raster shader READS
// MODEL_MATRIX: syn-camera-array-faces reads the `camera` auxiliary and
// synthesises its triangle from gl_VertexIndex.
//
// The oracle needs no camera and no transform. With nothing writing the raster
// node's transform3d port MODEL_MATRIX is identity, its translation column is
// (0,0,0), and the shaders' +0.5 bias makes that mid-grey. Both halves run the
// same chain and the same vertex source; the ONLY difference is MULTIVIEW:6
// plus the cubemap output it requires. So a mid-grey control next to a
// non-mid-grey multiview result isolates the binding shift and nothing else.
//
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_rawraster_model_matrix_binding
// =============================================================================
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Threedim/Camera.hpp>
#include <Threedim/Primitive.hpp>

#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>

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

//! Deliver control values to a Crousti node, as CroustiCpuNodes.cpp does.
void setInputs(score::gfx::Node& n, std::vector<ossia::value> vals)
{
  score::gfx::Message m;
  m.node_id = n.nodeId;
  for(auto& v : vals)
    m.input.push_back(std::move(v));
  n.process(std::move(m));
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

struct MmFacts
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;
  bool multiview_caps = false;
  ReadbackImage img;
};

// Cube -> ScenePreprocessor -> raw raster. `viewer` is the ISF pass the raster
// output is read through, or nullptr to read the raster output directly; the
// multiview half needs one because its output is a cubemap.
MmFacts run_chain(
    score::gfx::GraphicsApi api, const char* vs, const char* fs,
    const char* viewer)
{
  MmFacts f;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = score::test::new_document(app);
    if(!doc)
      return;
    auto& ctx = doc->context();
    HalpProcesses procs;
    GfxPipeline p;

    // The raster node draws the incoming mesh; both vertex shaders synthesise
    // their NDC from gl_VertexIndex and ignore where the cube actually lands.
    const int cube = p.addNode(procs.make<Threedim::Cube>(ctx));
    const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int prod = p.addRaster(corpus(vs), corpus(fs));
    if(cube < 0 || flat < 0 || prod < 0)
    {
      f.error = "chain build failed: " + p.error();
      return;
    }

    auto* cubeOut = p.nodeSceneOut(cube, 0);
    auto* flatIn = p.nodeSceneIn(flat, 0);
    auto* flatOut = p.nodeGeometryOut(flat, 0);
    if(!cubeOut || !flatIn || !flatOut)
    {
      f.error = "scene ports missing on the chain";
      return;
    }
    p.wire(cubeOut, flatIn);
    p.wire(flatOut, p.geometryIn(prod, 0));

    const int sink = p.addSink({192, 128});
    if(viewer)
    {
      const int view = p.addIsf(corpus(viewer));
      if(view < 0)
      {
        f.error = "viewer build failed: " + p.error();
        return;
      }
      p.wire(p.imageOut(prod, 0), p.imageIn(view, 0));
      p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    }
    else
    {
      p.wire(p.imageOut(prod, 0), p.sinkInput(sink));
    }

    if(!p.create(api))
    {
      f.backend = p.backend();
      f.skipped = p.skipped();
      f.skip_reason = p.skipReason();
      f.error = p.error();
      return;
    }
    f.backend = p.backend();
    if(!p.error().empty())
    {
      f.error = p.error();
      return;
    }

    p.render(4);
    f.img = p.readback(sink);

    auto& node = *p.isf(prod);
    for(auto& [renderList, renderer] : node.renderedNodes)
      if(renderList)
        f.multiview_caps = renderList->state.caps.multiview;
  });
  return f;
}

// Cube + Camera -> ScenePreprocessor -> raw raster drawing the mesh through
// MODEL_MATRIX alone. `eye` moves the camera; the returned image is what a
// shader written the "simple" way sees.
MmFacts run_with_camera(score::gfx::GraphicsApi api, float eyeZ)
{
  MmFacts f;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = score::test::new_document(app);
    if(!doc)
      return;
    auto& ctx = doc->context();
    HalpProcesses procs;
    GfxPipeline p;

    const int cube = p.addNode(procs.make<Threedim::Cube>(ctx));
    const int cam = p.addNode(procs.make<Threedim::Camera>(ctx));
    const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int prod
        = p.addRaster(corpus("syn-scene-solid.vs"), corpus("syn-scene-solid.fs"));
    if(cube < 0 || cam < 0 || flat < 0 || prod < 0)
    {
      f.error = "chain build failed: " + p.error();
      return;
    }
    auto* cubeOut = p.nodeSceneOut(cube, 0);
    auto* camOut = p.nodeSceneOut(cam, 0);
    auto* flatIn = p.nodeSceneIn(flat, 0);
    auto* flatOut = p.nodeGeometryOut(flat, 0);
    if(!cubeOut || !camOut || !flatIn || !flatOut)
    {
      f.error = "scene ports missing on the chain";
      return;
    }
    p.wire(cubeOut, flatIn);
    p.wire(camOut, flatIn);
    p.wire(flatOut, p.geometryIn(prod, 0));

    const int sink = p.addSink({160, 160});
    p.wire(p.imageOut(prod, 0), p.sinkInput(sink));

    // Camera::ins field order: eye, target, up, fov, near, far.
    setInputs(
        *p.node(cam), {ossia::value{ossia::vec3f{0.f, 0.f, eyeZ}},
                       ossia::value{ossia::vec3f{0.f, 0.f, 0.f}},
                       ossia::value{ossia::vec3f{0.f, 1.f, 0.f}}});

    if(!p.create(api))
    {
      f.backend = p.backend();
      f.skipped = p.skipped();
      f.skip_reason = p.skipReason();
      f.error = p.error();
      return;
    }
    f.backend = p.backend();
    if(!p.error().empty())
    {
      f.error = p.error();
      return;
    }
    p.render(4);
    f.img = p.readback(sink);
  });
  return f;
}
}

TEST_CASE(
    "MODEL_MATRIX reads identity with and without MULTIVIEW",
    "[gfx][l3][rawraster][multiview][modelmatrix]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  const MmFacts plain
      = run_chain(be, "syn-modelmat-tri.vs", "syn-modelmat-tri.fs", nullptr);
  if(plain.skipped)
    SKIP(plain.backend + ": " + plain.skip_reason);

  INFO("plain backend=" << plain.backend << " error='" << plain.error << "'");
  REQUIRE(plain.error.empty());
  REQUIRE(plain.img.valid());

  // Control. Identity MODEL_MATRIX -> translation 0 -> mid-grey. If this is not
  // mid-grey the shader is not reporting the matrix at all and the multiview
  // comparison below would be vacuous.
  const auto pc = plain.img.at(96, 64);
  INFO(
      "plain MODEL_MATRIX translation encodes as rgb "
      << (int)pc[0] << "," << (int)pc[1] << "," << (int)pc[2]
      << " (127/128 == identity, blue 255 == it drew)");
  REQUIRE(pc[2] >= 250);  // draw-witness first: no pixels means no oracle
  REQUIRE(pc[0] >= 120);
  REQUIRE(pc[0] <= 135);
  CHECK(pc[1] >= 120);
  CHECK(pc[1] <= 135);

  const MmFacts mv = run_chain(
      be, "syn-modelmat-tri-mv.vs", "syn-modelmat-tri-mv.fs",
      "syn-cube-six-probe.fs");
  if(mv.skipped)
    SKIP(mv.backend + ": " + mv.skip_reason);

  INFO("mv backend=" << mv.backend << " error='" << mv.error << "'");
  REQUIRE(mv.error.empty());
  REQUIRE(mv.img.valid());

  // Same gate as GfxCameraArrayFaces: procedural MULTIVIEW layered raster is
  // not renderable on headless GL or the null backend, so the pixel oracle only
  // means something where multiview actually amplifies.
  const bool isGL = mv.backend.find("OpenGL") != std::string::npos;
  const bool isNull = mv.backend.find("Null") != std::string::npos;
  if(isGL || isNull || !mv.multiview_caps)
  {
    SUCCEED(
        mv.backend
        + ": procedural MULTIVIEW layered raster is not renderable here; the "
          "control half above still pins the non-multiview binding.");
    return;
  }

  // The subject: every cube face must carry the same mid-grey. A binding shift
  // makes MODEL_MATRIX read whatever occupies the slot the renderer bound
  // instead, which is not an identity matrix.
  const int cw = mv.img.width / 3;
  const int ch = mv.img.height / 2;
  REQUIRE(cw > 0);
  REQUIRE(ch > 0);
  for(int face = 0; face < 6; face++)
  {
    const int col = face % 3;
    const int row = face / 3;
    const auto px = mv.img.at(col * cw + cw / 2, row * ch + ch / 2);
    INFO(
        "face " << face << " rgb " << (int)px[0] << "," << (int)px[1] << ","
                << (int)px[2] << " (blue 255 == this face drew)");
    // Blue is the draw-witness. If it is 255 the pass rendered and any
    // red/green miss is the matrix arriving from the wrong binding; if it is 0
    // the face never drew and says nothing about MODEL_MATRIX.
    CHECK(px[2] >= 250);
    CHECK(px[0] >= 120);
    CHECK(px[0] <= 135);
    CHECK(px[1] >= 120);
    CHECK(px[1] <= 135);
  }
}


// =============================================================================
// EXPECTED-RED. Moving a Camera must change what a raw-raster shader draws.
//
// There is no camera built-in for raw raster: the ISF prelude supplies only
// renderer_t (clipSpaceCorrMatrix, RENDERSIZE, MSAA_SAMPLES), process_t (TIME,
// ...) and raw-raster's own model_material_t. View and projection reach a
// shader ONLY through the ScenePreprocessor's `camera` auxiliary UBO, which the
// shader has to declare by name and index as a flat vec4 array against the
// 240-byte std140 CameraUBOData layout (15 vec4 per camera).
//
// So `clipSpaceCorrMatrix * MODEL_MATRIX * position` -- the idiom 33 corpus
// scores use -- contains no view or projection term at all, and the picture is
// identical wherever the camera stands. This case asserts the behaviour a user
// expects; it flips green when raw-raster grows VIEW/PROJECTION built-ins.
// =============================================================================
TEST_CASE(
    "moving a camera changes what a raw-raster shader draws",
    "[gfx][l3][rawraster][camera][!shouldfail]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  const MmFacts near_ = run_with_camera(be, 3.f);
  const MmFacts far_ = run_with_camera(be, 12.f);
  if(near_.skipped || far_.skipped)
    SKIP(near_.backend + ": " + near_.skip_reason);

  INFO("near error='" << near_.error << "' far error='" << far_.error << "'");
  REQUIRE(near_.error.empty());
  REQUIRE(far_.error.empty());
  REQUIRE(near_.img.valid());
  REQUIRE(far_.img.valid());

  // A cube seen from z=3 and from z=12 cannot cover the same pixels.
  int differing = 0;
  for(int y = 0; y < near_.img.height; y += 4)
    for(int x = 0; x < near_.img.width; x += 4)
    {
      const auto a = near_.img.at(x, y);
      const auto b = far_.img.at(x, y);
      if(a[0] != b[0] || a[1] != b[1] || a[2] != b[2])
        differing++;
    }
  INFO("pixels differing between eye z=3 and z=12: " << differing);
  CHECK(differing > 0);
}
