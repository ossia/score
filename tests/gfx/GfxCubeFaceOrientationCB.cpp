// Cube faces rendered by a raw raster land in the order of the GL cube-map face
// table (4.6, Table 8.19) on every backend.
//
// Two captures write, into every texel of a CUBEMAP output, the encoding of the
// world direction that texel stands for (rgb = dir * 0.5 + 0.5):
//   - camera-driven: a Threedim::CameraArray through the Scene Preprocessor,
//     each face drawn through its own camera's viewProjection
//     (cb-cube-dir-camera.*), the way classic_pbr_multiview / cubemap_pbr
//     capture a scene;
//   - NDC-derived: the direction computed from the pre-clipSpaceCorrMatrix NDC
//     position with face row 0 at NDC y = -1 (cb-cube-dir-ndc.*), the way the
//     csf-examples IBL presets fill a cube.
// Each is rendered once with MULTIVIEW:6 and once with EXECUTION_MODEL
// PER_CUBE_FACE, the lane that runs on OpenGL. cb-cube-dir-probe.fs samples 24
// off-centre directions (four per face) and a correct cube returns each
// direction's own encoding.
//
// Vulkan's clipSpaceCorrMatrix alone put face row 0 at NDC y = +1, the reverse
// of OpenGL (and of D3D / Metal, whose raw-raster epilogue negates y), so both
// captures came out mirrored along t on Vulkan: all 24 probes wrong.
//
// The NDC captures run again with back faces culled: the negated y reverses
// window-space winding, so the pipeline front face is swapped with it, as it
// already is on D3D and Metal; without the swap the counter-clockwise
// fullscreen triangle is culled and the cube stays black.
//
// Registration: see the test_gfx_cube_face_orientation_cb target.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>
#include <Threedim/CameraArray.hpp>
#include <Threedim/Primitive.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <cmath>
#include <memory>
#include <sstream>
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

// The sink is square: packCameraUBO takes the projection aspect from the
// render size.
constexpr int kColumns = 24;
constexpr int kCellW = 8;
constexpr int kTol = 8;

std::array<float, 3> probeDirection(int c)
{
  const int face = c / 4;
  const int q = c % 4;
  const float s1 = (q & 1) ? 1.f : -1.f;
  const float s2 = (q & 2) ? 1.f : -1.f;
  const float sg = (face % 2) == 0 ? 1.f : -1.f;
  if(face < 2)
    return {sg, 0.55f * s1, 0.3f * s2};
  if(face < 4)
    return {0.55f * s1, sg, 0.3f * s2};
  return {0.55f * s1, 0.3f * s2, sg};
}

std::array<uint8_t, 4> expected(int c)
{
  const auto d = probeDirection(c);
  const float n = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  std::array<uint8_t, 4> out{0, 0, 0, 255};
  for(int k = 0; k < 3; ++k)
    out[k] = uint8_t(std::lround((d[k] / n * 0.5f + 0.5f) * 255.f));
  return out;
}

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

struct Capture
{
  const char* name;
  const char* vs;
  const char* fs;
  bool camera;
  bool multiview;
};

const Capture kCaptures[] = {
    {"camera multiview", "cb-cube-dir-camera.vs", "cb-cube-dir-camera.fs", true, true},
    {"camera per-cube-face", "cb-cube-dir-camera-perface.vs",
     "cb-cube-dir-camera-perface.fs", true, false},
    {"ndc multiview", "cb-cube-dir-ndc.vs", "cb-cube-dir-ndc.fs", false, true},
    {"ndc per-cube-face", "cb-cube-dir-ndc-perface.vs", "cb-cube-dir-ndc-perface.fs",
     false, false},
    {"ndc multiview, back faces culled", "cb-cube-dir-ndc.vs", "cb-cube-dir-ndc-cull.fs",
     false, true},
    {"ndc per-cube-face, back faces culled", "cb-cube-dir-ndc-perface.vs",
     "cb-cube-dir-ndc-perface-cull.fs", false, false},
};

struct Outcome
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;
  bool multiview_caps = false;
  ReadbackImage view;
};

Outcome run(score::gfx::GraphicsApi api, const Capture& cap)
{
  Outcome out;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* document = score::test::new_document(app);
    if(!document)
    {
      out.error = "could not create a document";
      return;
    }
    const score::DocumentContext& ctx = document->context();
    HalpProcesses procs;
    GfxPipeline p;
    const int prod = p.addRaster(corpus(cap.vs), corpus(cap.fs));
    const int probe = p.addIsf(corpus("cb-cube-dir-probe.fs"));
    if(prod < 0 || probe < 0)
    {
      out.error = "chain build failed: " + p.error();
      return;
    }
    if(cap.camera)
    {
      const int cube = p.addNode(procs.make<Threedim::Cube>(ctx));
      const int cams = p.addNode(procs.make<Threedim::CameraArray>(ctx));
      const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
      auto* cubeOut = p.nodeSceneOut(cube, 0);
      auto* camsOut = p.nodeSceneOut(cams, 0);
      auto* flatIn = p.nodeSceneIn(flat, 0);
      auto* flatOut = p.nodeGeometryOut(flat, 0);
      if(!cubeOut || !camsOut || !flatIn || !flatOut)
      {
        out.error = "scene ports missing on the chain";
        return;
      }
      p.wire(cubeOut, flatIn);
      p.wire(camsOut, flatIn);
      p.wire(flatOut, p.geometryIn(prod, 0));
      setInputs(
          *p.node(cams), {ossia::value{ossia::vec3f{0.f, 0.f, 0.f}},
                          ossia::value{0.1f}, ossia::value{100.f}});
    }
    const int sink = p.addSink({kColumns * kCellW, kColumns * kCellW});
    p.wire(p.imageOut(prod, 0), p.imageIn(probe, 0));
    p.wire(p.imageOut(probe, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      out.backend = p.backend();
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    out.backend = p.backend();
    p.render(4);
    out.view = p.readback(sink);
    for(auto& [renderList, renderer] : p.isf(prod)->renderedNodes)
      if(renderList)
        out.multiview_caps = renderList->state.caps.multiview;
    if(!out.view.valid())
      out.error = "empty readback";
  });
  return out;
}
}

TEST_CASE(
    "raw-raster cube captures land in GL cube-map face order",
    "[gfx][rawraster][cubemap][orientation]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto ci = GENERATE(range(0, int(std::size(kCaptures))));
  const Capture& cap = kCaptures[ci];
  CAPTURE(backend_name(api), cap.name);

  const Outcome out = run(api, cap);
  if(out.skipped)
    SKIP(out.skip_reason);
  REQUIRE(out.error.empty());
  if(cap.multiview && (!out.multiview_caps || api == score::gfx::OpenGL))
  {
    SUCCEED(
        out.backend
        + ": a MULTIVIEW layered raster is not renderable here (on OpenGL it "
          "reads back black offscreen, see GfxCameraArrayFaces.cpp); the "
          "PER_CUBE_FACE captures cover this backend");
    return;
  }

  int wrong = 0;
  std::ostringstream log;
  for(int c = 0; c < kColumns; ++c)
  {
    const auto got = out.view.at(c * kCellW + kCellW / 2, out.view.height / 2);
    const auto want = expected(c);
    if(!near(got, want, kTol))
    {
      ++wrong;
      log << " [" << c << "] got (" << int(got[0]) << "," << int(got[1]) << ","
          << int(got[2]) << ") want (" << int(want[0]) << "," << int(want[1]) << ","
          << int(want[2]) << ")";
    }
  }
  INFO(out.backend << ":" << log.str());
  CHECK(wrong == 0);
}
