// =============================================================================
// A CAMERA ARRAY RENDERS SIX FACES.
//
// Threedim::CameraArray publishes six cameras — +X, -X, +Y, -Y, +Z, -Z at
// yfov = pi/2. ScenePreprocessor flattens them, packs them into a std140
// CameraUBOData array and publishes that array as the geometry-borne auxiliary
// buffer named "camera". A MULTIVIEW:6 raw-raster pass then indexes
// camera[gl_ViewIndex] and must get FACE gl_ViewIndex — not face 0 six times,
// not the faces in some other order.
//
// ctest name `test_gfx_camera_array_faces`. Threedim::CameraArray and
// Threedim::Cube are hidden-visibility inside score_plugin_threedim, so
// CameraArray.cpp and Primitive.cpp are compiled into the test target, the way
// test_gfx_crousti_cpu_nodes does it.
//
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_camera_array_faces
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_camera_array_faces
//
// -----------------------------------------------------------------------------
// RELATION TO KnownDefects.cpp #163
// -----------------------------------------------------------------------------
// #163 is the "camera" aux advertising one entry for N cameras. Both
// `.name = "camera"` publication sites in ScenePreprocessorNode.cpp size their
// extent with cameraAuxByteSize(m_cachedCameras.size()) =
// max(1, cameraCount) * sizeof(CameraUBOData), and the consumer prefers the
// aux's byte_size over the buffer wrapper's, falling back only when it is <= 0
// — which cameraAuxByteSize never returns. The underlying QRhiBuffer is
// pre-allocated to max(bytes, 16 * 240) = 3840 B minimum and all N entries are
// uploaded. So the 1440-byte camera[6] block this test's shader declares is
// backed by a 1440-byte binding, and this case is a green regression guard, not
// an expected-red pin: if either site reverts to `sizeof(CameraUBOData)` the
// binding shrinks to 240 bytes under a 1440-byte block and views 1..5 read out
// of range.
//
// -----------------------------------------------------------------------------
// THE CLOSED FORM — six view directions => six predicted colours
// -----------------------------------------------------------------------------
// (1) CameraArray.hpp declares, in GL cubemap face order:
//         face 0 +X forward ( 1, 0, 0)   face 3 -Y forward ( 0,-1, 0)
//         face 1 -X forward (-1, 0, 0)   face 4 +Z forward ( 0, 0, 1)
//         face 2 +Y forward ( 0, 1, 0)   face 5 -Z forward ( 0, 0,-1)
// (2) it builds each node's rotation as QQuaternion::fromDirection(-forward,
//     up). Qt's fromDirection makes `direction` the rotation's third basis
//     vector, so the world-space matrix R has column 2 == -forward (GL cameras
//     look down local -Z).
// (3) flattenScene composes that into CameraEntry::worldTransform (scale is 1,
//     so the 3x3 part is exactly R), and packCameraUBO writes
//     view = worldTransform.inverted(). For a rigid transform the rotation part
//     of the inverse is R^T.
// (4) QMatrix4x4::constData() is column-major and writeMat4 memcpys it; std140
//     mat4 is column-major too. So in GLSL
//     view[c][r] == (R^T)[r][c] == R[c][r], hence
//         vec3(view[0][2], view[1][2], view[2][2]) == R's column 2 == -forward
//     and the shader recovers  forward = -vec3(view[0][2], view[1][2], view[2][2]).
//     (syn-camera-array-faces.vs reads those three components as
//      data[base+0].z, data[base+1].z, data[base+2].z.)
// (5) The shader's own encoding is  rgb = forward * 0.5 + 0.5.
//     Every component of a unit axis is exactly -1, 0 or +1, so the encoded
//     value is exactly 0.0, 0.5 or 1.0. rgba8 unorm: 0.0 -> 0, 1.0 -> 255,
//     0.5 -> 127.5, which a conforming implementation may round either way
//     (hence tol 4 below, and 128 written as the nominal).
//
//     face 0 +X : (+1, 0, 0) -> (1.0, 0.5, 0.5) -> (255, 128, 128)
//     face 1 -X : (-1, 0, 0) -> (0.0, 0.5, 0.5) -> (  0, 128, 128)
//     face 2 +Y : ( 0,+1, 0) -> (0.5, 1.0, 0.5) -> (128, 255, 128)
//     face 3 -Y : ( 0,-1, 0) -> (0.5, 0.0, 0.5) -> (128,   0, 128)
//     face 4 +Z : ( 0, 0,+1) -> (0.5, 0.5, 1.0) -> (128, 128, 255)
//     face 5 -Z : ( 0, 0,-1) -> (0.5, 0.5, 0.0) -> (128, 128,   0)
//
// Nothing here was read off a rendered image. The CPU lane below recomputes the
// same six triples from Threedim::CameraArray + flattenScene + packCameraUBO on
// the CPU and requires them to equal this table, so the table is checked by two
// independent derivations before a single pixel is compared against it.
//
// -----------------------------------------------------------------------------
// EVERY FACE FOUND EXACTLY ONCE — and why the histogram is not enough
// -----------------------------------------------------------------------------
// Two different assertions run, and only one of them pins identity:
//   (a) per-face equality: probe cell f == the colour predicted for face f.
//       THIS is what pins which face is which.
//   (b) histogram: each of the six colours appears exactly once across the six
//       probes. This catches drops and duplications (one layer blitted into
//       every face; one camera packed six times) but is BLIND to a permutation
//       — the +X/-X swap used as this case's negative control leaves (b)
//       completely green. It is kept because it catches a different failure
//       class, never as a substitute for (a).
//
// -----------------------------------------------------------------------------
// NEGATIVE CONTROL (product-side, do not commit)
// -----------------------------------------------------------------------------
// Swap the two forward vectors of the first two rows of the `kFaces` table
// inside CameraArray.hpp's rebuild(), leaving everything else alone:
//
//       {{-1.f,  0.f,  0.f}, {0.f, -1.f,  0.f}},  // +X slot, now looking -X
//       {{ 1.f,  0.f,  0.f}, {0.f, -1.f,  0.f}},  // -X slot, now looking +X
//
// (Only the .hpp copy: rebuild() is the scene_spec path this test drives.
//  CameraArray.cpp holds a SECOND copy of the same table, used for the
//  RawCamera/RawTransform arena slots, under a "keep the two definitions in
//  sync" comment — editing one and not the other is itself a latent hazard.)
//
// Must go RED:
//   * CPU lane, "the six faces are the six axes": the face-0 and face-1
//     direction assertions (2 of 6).
//   * CPU lane, "packCameraUBO reproduces the six predicted colours": the
//     face-0 and face-1 colour assertions (2 of 6).
//   * GPU lane, per-face equality (a): cells 0 and 1 (2 of 6) on every
//     multiview-capable backend.
// Must stay GREEN:
//   * All four remaining faces in each of those groups.
//   * The histogram (b), on both lanes — both colours are still present exactly
//     once, they have merely traded places. This is the point of keeping (a).
//   * Every structural assertion (six cameras, active index 0, cube handle,
//     shim selection): the swap changes orientation, not cardinality or
//     plumbing.
//
// -----------------------------------------------------------------------------
// HARDWARE / BACKEND SCOPE — following the GfxMultiview.cpp precedent
// -----------------------------------------------------------------------------
// MULTIVIEW is a Vulkan / D3D12 (/Metal) feature.
//   * Vulkan / D3D12 / Metal with caps.multiview: the full six-colour pixel
//     oracle runs.
//   * OpenGL: GfxMultiview.cpp records that the offscreen GL context does not
//     render a procedural MULTIVIEW layered raster at all — layer 0 reads back
//     black even on the FIXED engine — and Qt applies
//     ovr_multiview_view_count to the vertex stage only, so these cases must
//     not assume multiview bakes on GL.
//     GfxCubemapSixFaces.cpp turns that into the "structural lane": assert the
//     decision logic (a cube handle is published, the array-then-copy shim was
//     selected) and SUCCEED with the reason instead of pretending to have
//     pixels. Same here, plus the CPU lane, which is the strongest part of
//     this case that is hardware-independent.
//   * Null / no multiview caps / no usable RHI: SKIP or structural lane only.
//     Never a pixel verdict: do not fall back to Null for a case whose
//     verdict is a pixel.
//
// -----------------------------------------------------------------------------
// WHY A NEW CORPUS PAIR
// -----------------------------------------------------------------------------
// syn-cube-six-colors.{vs,fs} does NOT fit: it colours each
// face from a CONSTANT table indexed by gl_ViewIndex and never touches the
// camera UBO, so it cannot distinguish "six cameras reached six views" from "no
// camera reached anything". syn-raster-per-cube-face.{vs,fs} is EXECUTION_MODEL
// PER_CUBE_FACE (six passes keyed on PASSINDEX), not MULTIVIEW, and also
// ignores the camera. The camera probe in GfxEnvRenderTargetSize.cpp reads the
// camera block as a single struct (slot 0 only) and is not multiview. No
// committed shader reads the camera aux as an ARRAY. Hence exactly one new
// pair, syn-camera-array-faces.{vs,fs}; its READ side reuses the existing
// syn-cube-six-probe.fs viewer unchanged.
// =============================================================================
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Threedim/CameraArray.hpp>
#include <Threedim/Primitive.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <Gfx/Graph/CameraMath.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <QMatrix4x4>
#include <QSize>
#include <QVector3D>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace score::test;
using namespace score::test::gfx;
using Catch::Approx;

namespace
{
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

// The sink is SQUARE on purpose. packCameraUBO derives the projection aspect
// from renderSize and only falls back to camera_component::aspect_ratio when
// renderSize.height() <= 0, so CameraArray's declared
// aspect_ratio = 1 does NOT by itself make the projection
// square — the render target has to be square too. The direction oracle below
// is aspect-independent, but keeping the sink square lets the CPU lane also pin
// projection[0][0] == 1 (see kProj00Square) instead of leaving yfov unchecked.
// 96x96 => the 3x2 probe grid has 32x48 cells.
constexpr int kSinkW = 96;
constexpr int kSinkH = 96;

// yfov = pi/2 => fovYDeg = 90 => cot(45 deg) = 1; setReverseZPerspective sets
// projection(0,0) = cot / aspect, aspect = 96/96 = 1.
constexpr float kProj00Square = 1.f;

// GL cubemap face order, the order CameraArray.hpp declares.
const char* const kFaceNames[6] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};

// Step (1) of the closed form in the header. Nothing else in this file
// hard-codes a direction; both the CPU lane and the pixel table derive from
// here.
constexpr std::array<std::array<float, 3>, 6> kForward{{
    {{1.f, 0.f, 0.f}},   // 0 +X
    {{-1.f, 0.f, 0.f}},  // 1 -X
    {{0.f, 1.f, 0.f}},   // 2 +Y
    {{0.f, -1.f, 0.f}},  // 3 -Y
    {{0.f, 0.f, 1.f}},   // 4 +Z
    {{0.f, 0.f, -1.f}},  // 5 -Z
}};

// Step (5): the shader's own encoding, applied to a component of a unit axis.
// -1 -> 0.0 -> byte 0 ; 0 -> 0.5 -> byte 127.5 (nominal 128) ; +1 -> 1.0 -> 255.
constexpr uint8_t encodeAxisComponent(float c) noexcept
{
  return c < -0.5f ? uint8_t(0) : (c > 0.5f ? uint8_t(255) : uint8_t(128));
}

// The six predicted colours, computed from kForward by the shader's encoding.
std::array<uint8_t, 4> predictedColour(int face) noexcept
{
  const auto& f = kForward[std::size_t(face)];
  return {
      encodeAxisComponent(f[0]), encodeAxisComponent(f[1]),
      encodeAxisComponent(f[2]), 255};
}

// 0.5 lands on 127.5, which rgba8 rounding may take either way, and the value
// makes two rgba8 round trips (cube face, then the viewer's own output).
constexpr int kTol = 4;

// --- Crousti glue (cloned from CroustiCpuNodes.cpp) -------------------------

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

//! Deliver control values to a Crousti node, as CroustiCpuNodes.cpp does.
void setInputs(score::gfx::Node& n, std::vector<ossia::value> vals)
{
  score::gfx::Message m;
  m.node_id = n.nodeId;
  for(auto& v : vals)
    m.input.push_back(std::move(v));
  n.process(std::move(m));
}

//! Recover the world-space forward direction from a camera's world transform,
//! the same way the shader recovers it from `view` — see steps (2)-(4) of the
//! header. R's column 2 is the camera's local +Z in world; GL cameras look down
//! local -Z, so forward = -column2.
QVector3D forwardOf(const QMatrix4x4& worldTransform)
{
  return -worldTransform.column(2).toVector3D();
}

//! ... and the same recovery done on the PACKED std140 bytes, i.e. exactly the
//! expression syn-camera-array-faces.vs evaluates. Guards step (4): if Qt's
//! matrix storage or std140's column-major convention ever stopped agreeing,
//! this and forwardOf() would disagree.
QVector3D forwardOfPacked(const score::gfx::CameraUBOData& d)
{
  // view[c][r] lives at d.view[c * 4 + r].
  return -QVector3D{d.view[0 * 4 + 2], d.view[1 * 4 + 2], d.view[2 * 4 + 2]};
}

struct FaceFacts
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;

  // Structural (pixel-free) half — the lane that runs on GL / Null, the
  // pattern of GfxCubemapSixFaces.cpp.
  bool multiview_caps = false;
  // caps.multiview alone does not mean multiview is USED: a D3D target whose
  // shader model cannot compile SV_ViewID has the capability and still falls
  // back to the per-pass path. See viewIndexNeedsPassIndexFallback().
  bool multiview_lowered = false;
  bool renderer_found = false;
  bool out_tex_found = false;
  bool out_tex_is_cube = false;
  bool out_tex_is_shim_cube = false;

  ReadbackImage view; // syn-cube-six-probe.fs's 3x2 grid
};

FaceFacts run_faces(score::gfx::GraphicsApi api)
{
  FaceFacts f;
  f.backend = backend_name(api);
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* document = score::test::new_document(app);
    if(!document)
    {
      f.error = "could not create a document (ProcessModel needs one)";
      return;
    }
    const score::DocumentContext& ctx = document->context();

    HalpProcesses procs;
    GfxPipeline p;

    // A Cube is in the scene for one reason only: the raw-raster consumer
    // resolves its "camera" uniform by NAME against the incoming geometry's
    // auxiliary list, so there has to be geometry on the edge. Its placement is irrelevant — the vertex
    // shader synthesises a fullscreen triangle and ignores `position`.
    const int cube = p.addNode(procs.make<Threedim::Cube>(ctx));
    const int cams = p.addNode(procs.make<Threedim::CameraArray>(ctx));
    const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int prod = p.addRaster(
        corpus("syn-camera-array-faces.vs"), corpus("syn-camera-array-faces.fs"));
    // Samples the six axis directions of the
    // upstream cubemap into a 3x2 grid, each face exactly once.
    const int view = p.addIsf(corpus("syn-cube-six-probe.fs"));
    if(cube < 0 || cams < 0 || flat < 0 || prod < 0 || view < 0)
    {
      f.error = "chain build failed: " + p.error();
      return;
    }

    auto* cubeOut = p.nodeSceneOut(cube, 0);
    auto* camsOut = p.nodeSceneOut(cams, 0);
    auto* flatIn = p.nodeSceneIn(flat, 0);
    auto* flatOut = p.nodeGeometryOut(flat, 0);
    if(!cubeOut || !camsOut || !flatIn || !flatOut)
    {
      f.error = "scene ports missing on the chain";
      return;
    }
    p.wire(cubeOut, flatIn);
    p.wire(camsOut, flatIn);
    p.wire(flatOut, p.geometryIn(prod, 0));

    const int sink = p.addSink({kSinkW, kSinkH});
    p.wire(p.imageOut(prod, 0), p.imageIn(view, 0));
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));

    // Pin the controls so the oracle cannot drift with a default change.
    // CameraArray::ins field order: origin, near, far.
    // The origin does not enter the direction oracle (it is the translation
    // column, not the rotation), but pinning it keeps the eye at the world
    // origin, which is what a cubemap probe array means.
    setInputs(
        *p.node(cams), {ossia::value{ossia::vec3f{0.f, 0.f, 0.f}},
                        ossia::value{0.1f}, ossia::value{100.f}});

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
    f.view = p.readback(sink);

    // Structural half, harvested through the public NodeRenderer API while the
    // pipeline is alive — the same way GfxCubemapSixFaces.cpp does it.
    auto* outPort = p.imageOut(prod, 0);
    auto& node = *p.isf(prod);
    for(auto& [renderList, renderer] : node.renderedNodes)
    {
      if(!renderList || !renderer)
        continue;
      f.renderer_found = true;
      f.multiview_caps = renderList->state.caps.multiview;
      // Both corpus shaders declare MULTIVIEW:6 -- a cubemap is six views by
      // definition, which is also why D3D12 ViewInstancing (max 4) cannot
      // serve them. The count is part of the predicate, so pass it.
      f.multiview_lowered = score::gfx::viewIndexNeedsPassIndexFallback(
          renderList->state.api, renderList->state.version, 6);
      if(QRhiTexture* tex = renderer->textureForOutput(*outPort))
      {
        f.out_tex_found = true;
        f.out_tex_is_cube = tex->flags().testFlag(QRhiTexture::CubeMap);
        f.out_tex_is_shim_cube = tex->name().contains("cubeCopyCube");
      }
    }
  });
  return f;
}
} // namespace

// =============================================================================
// CPU lane — no GPU, no RHI, runs on every machine including GPU-less CI.
//
// This is the half that pins WHICH face is which at the source. It rebuilds a
// Threedim::CameraArray, runs the real flattenScene and the real packCameraUBO,
// and re-derives the six predicted colours independently of the shader. If this
// lane is green and the GPU lane's per-face equality is red, the fault is in
// the transport (packing order / aux extent / view amplification), not in the
// camera maths — which is exactly the split this case needs to be diagnosable.
// =============================================================================

TEST_CASE(
    "Camera Array's six faces are the six axes, each exactly once, at yfov pi/2",
    "[gfx][threedim][scene][camera][multiview][p2-1]")
{
  // Aggregate value-init: the GpuResourceRegistry::Slot arrays are only ever
  // touched by init()/update()/release(), which this GPU-free lane never calls.
  Threedim::CameraArray arr{};
  // Defaults per CameraArray.hpp: origin (0,0,0), near 0.1, far 1000.
  arr.rebuild();
  arr();
  REQUIRE(arr.outputs.scene_out.scene.state);

  score::gfx::FlatScene fs;
  // aspectRatio here is flattenScene's legacy-mirror argument; the per-camera
  // projection is (re)built by packCameraUBO from renderSize, not from this.
  score::gfx::flattenScene(
      arr.outputs.scene_out.scene, fs, float(kSinkW) / float(kSinkH));

  // Cardinality first: six cameras, no more, no fewer.
  REQUIRE(fs.cameras.size() == 6);

  // packAndUploadCameras hoists the ACTIVE camera to slot 0 and appends the
  // rest in insertion order. That reorder is a no-op here only because
  // CameraArray declares face 0 active, which flattenScene resolves to
  // activeCameraIndex 0. Pinned explicitly: if the active camera were
  // ever anything but face 0, camera[gl_ViewIndex] would stop being face
  // gl_ViewIndex and the whole multiview correspondence would rotate.
  CHECK(fs.activeCameraIndex == 0);

  for(int i = 0; i < 6; ++i)
  {
    INFO("face " << kFaceNames[i] << " (" << i << ")");
    const auto& e = fs.cameras[std::size_t(i)];
    REQUIRE(e.component);

    // yfov / aspect_ratio for the six faces.
    CHECK(e.component->yfov == Approx(float(M_PI) / 2.f));
    CHECK(e.component->aspect_ratio == Approx(1.f));

    // (2)-(3): the recovered forward is this face's axis.
    const QVector3D fwd = forwardOf(e.worldTransform);
    CHECK(fwd.x() == Approx(kForward[std::size_t(i)][0]).margin(1e-4));
    CHECK(fwd.y() == Approx(kForward[std::size_t(i)][1]).margin(1e-4));
    CHECK(fwd.z() == Approx(kForward[std::size_t(i)][2]).margin(1e-4));

    // The eye sits at the pinned origin: the rotation carries the face, the
    // translation does not.
    const QVector3D eye = e.worldTransform.column(3).toVector3D();
    CHECK(eye.length() == Approx(0.f).margin(1e-4));
  }

  // Histogram (b): each axis appears exactly once. Blind to a permutation by
  // construction — see the header. Kept for the drop/duplicate class.
  for(int want = 0; want < 6; ++want)
  {
    int found = 0;
    for(int i = 0; i < 6; ++i)
    {
      const QVector3D fwd = forwardOf(fs.cameras[std::size_t(i)].worldTransform);
      if(std::abs(fwd.x() - kForward[std::size_t(want)][0]) < 1e-4f
         && std::abs(fwd.y() - kForward[std::size_t(want)][1]) < 1e-4f
         && std::abs(fwd.z() - kForward[std::size_t(want)][2]) < 1e-4f)
        ++found;
    }
    INFO("direction " << kFaceNames[want] << " found " << found << " time(s)");
    CHECK(found == 1);
  }
}

TEST_CASE(
    "packCameraUBO reproduces the six predicted colours the multiview shader "
    "will write",
    "[gfx][threedim][scene][camera][multiview][p2-1]")
{
  // Aggregate value-init: the GpuResourceRegistry::Slot arrays are only ever
  // touched by init()/update()/release(), which this GPU-free lane never calls.
  Threedim::CameraArray arr{};
  arr.rebuild();
  arr();
  REQUIRE(arr.outputs.scene_out.scene.state);

  score::gfx::FlatScene fs;
  score::gfx::flattenScene(
      arr.outputs.scene_out.scene, fs, float(kSinkW) / float(kSinkH));
  REQUIRE(fs.cameras.size() == 6);

  // Pack exactly as ScenePreprocessorNode's packAndUploadCameras does, in the
  // order it does (active first, then the rest) — here that is plain face
  // order.
  std::vector<score::gfx::CameraUBOData> packed;
  packed.reserve(6);
  const int active = std::max(0, fs.activeCameraIndex);
  auto packOne = [&](const score::gfx::FlatScene::CameraEntry& e) {
    score::gfx::CameraUBOData d{};
    score::gfx::packCameraUBO(
        d, *e.component, e.worldTransform, QSize{kSinkW, kSinkH}, 0.f);
    packed.push_back(d);
  };
  packOne(fs.cameras[std::size_t(active)]);
  for(std::size_t i = 0; i < fs.cameras.size(); ++i)
    if(int(i) != active)
      packOne(fs.cameras[i]);
  REQUIRE(packed.size() == 6);

  // The block the shader declares is 6 * 240 = 1440 bytes; see the #163
  // paragraph in the header. cameraAuxByteSize is what both publication sites
  // advertise for it now.
  CHECK(sizeof(score::gfx::CameraUBOData) == 240u);
  CHECK(score::gfx::cameraAuxByteSize(6) == 1440);
  CHECK(score::gfx::cameraAuxByteSize(1) == 240);

  for(int i = 0; i < 6; ++i)
  {
    INFO("face " << kFaceNames[i] << " (" << i << ")");
    const auto& d = packed[std::size_t(i)];

    // Step (4) on real packed bytes: the shader's expression and the
    // matrix-level one must agree.
    const QVector3D fwd = forwardOfPacked(d);
    CHECK(fwd.x() == Approx(kForward[std::size_t(i)][0]).margin(1e-4));
    CHECK(fwd.y() == Approx(kForward[std::size_t(i)][1]).margin(1e-4));
    CHECK(fwd.z() == Approx(kForward[std::size_t(i)][2]).margin(1e-4));

    // Step (5): the byte triple the fragment stage will emit.
    const std::array<uint8_t, 4> want = predictedColour(i);
    const std::array<uint8_t, 4> got{
        uint8_t(std::lround(std::clamp(fwd.x() * 0.5f + 0.5f, 0.f, 1.f) * 255.f)),
        uint8_t(std::lround(std::clamp(fwd.y() * 0.5f + 0.5f, 0.f, 1.f) * 255.f)),
        uint8_t(std::lround(std::clamp(fwd.z() * 0.5f + 0.5f, 0.f, 1.f) * 255.f)),
        uint8_t(255)};
    INFO(
        "encoded (" << int(got[0]) << "," << int(got[1]) << "," << int(got[2])
                    << ") vs predicted (" << int(want[0]) << "," << int(want[1])
                    << "," << int(want[2]) << ")");
    CHECK(near(got, want, kTol));

    // yfov reaches the projection: with a SQUARE render target,
    // projection[0][0] = cot(yfov/2) / 1 = cot(45 deg) = 1. This is the only
    // place the pi/2 field of view is checked numerically rather than as a
    // stored control value. With a NON-square render target this would be
    // 1/aspect — packCameraUBO ignores camera_component::aspect_ratio whenever
    // renderSize.height() > 0, so CameraArray's declared aspect_ratio = 1 does
    // not survive to the projection on its own. Documented, not a defect pin.
    CHECK(d.projection[0] == Approx(kProj00Square).margin(1e-4));
  }

  // Histogram (b) on the encoded colours.
  for(int want = 0; want < 6; ++want)
  {
    const auto w = predictedColour(want);
    int found = 0;
    for(int i = 0; i < 6; ++i)
    {
      const QVector3D f = forwardOfPacked(packed[std::size_t(i)]);
      const std::array<uint8_t, 4> got{
          encodeAxisComponent(f.x()), encodeAxisComponent(f.y()),
          encodeAxisComponent(f.z()), 255};
      if(near(got, w, kTol))
        ++found;
    }
    INFO("colour for " << kFaceNames[want] << " found " << found << " time(s)");
    CHECK(found == 1);
  }
}

// =============================================================================
// GPU lane — the real chain: CameraArray + Cube -> ScenePreprocessor ->
// MULTIVIEW:6 + CUBEMAP raw raster -> samplerCube viewer -> readback.
// =============================================================================

TEST_CASE(
    "a camera array renders six faces",
    "[gfx][l3][scene][camera][multiview][cubemap][p2-1]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  const FaceFacts f = run_faces(be);
  if(f.skipped)
  {
    // Never fail for environment.
    SKIP(f.backend + ": " + f.skip_reason);
  }

  INFO("backend=" << f.backend << " error='" << f.error << "'");
  REQUIRE(f.error.empty());
  REQUIRE(f.view.valid());

  // ---- Structural (pixel-free) half. Holds wherever the pipeline built.
  REQUIRE(f.renderer_found);
  REQUIRE(f.out_tex_found);
  // Downstream samplerCube consumers bind textureForOutput's result directly.
  CHECK(f.out_tex_is_cube);
  if(f.multiview_caps && !f.multiview_lowered)
  {
    // QRhi forbids setMultiViewCount on a cube texture, so MULTIVIEW+CUBEMAP
    // must go through the array-then-copy shim and publish its cube, not the
    // shadow array it actually renders into.
    CHECK(f.out_tex_is_shim_cube);
  }

  // ---- Pixel half, gated exactly as the header's backend scope says.
  const bool isGL = f.backend.find("OpenGL") != std::string::npos;
  const bool isNull = f.backend.find("Null") != std::string::npos;
  // ... unless the pass-index fallback is active, in which case the six
  // faces ARE written -- as N explicit passes rather than one amplified
  // draw -- so the oracle is expressible and must run. Without this branch a
  // backend with caps.multiview == 0 (d3d11) passes on a fraction of the
  // assertions, and that vacuous green hides a missing-faces fault.
  if(isGL || isNull || (!f.multiview_caps && !f.multiview_lowered))
  {
    SUCCEED(
        f.backend
        + ": procedural MULTIVIEW layered raster is not renderable here "
          "(GfxMultiview.cpp:64-77 measured it black even post-fix on headless "
          "GL; Qt bakes ovr_multiview_view_count into the vertex stage only, "
          "qspirvshader.cpp:954). Structural lane asserted above, the six "
          "camera directions are pinned by this file's CPU lane, and the "
          "six-colour pixel oracle runs on Vulkan/D3D12/Metal.");
    return;
  }

  const auto& img = f.view;
  const int cw = img.width / 3;
  const int ch = img.height / 2;
  REQUIRE(cw > 0);
  REQUIRE(ch > 0);

  // Probe cell centres. syn-cube-six-probe.fs keys its rows on
  // isf_FragNormCoord.y < 0.5, and under the house ISF orientation contract
  // uv.y == 1 is the TOP row of the delivered image, so shader row 0
  // (faces 0..2) lands in the BOTTOM half of the readback. That flip is not a
  // guess: GfxCubemapSixFaces.cpp records the same flip -- reading top-first
  // swaps 0<->3, 1<->4, 2<->5 while all six colours stay present exactly once.
  // Same viewer, same correction.
  std::array<std::array<uint8_t, 4>, 6> got{};
  for(int face = 0; face < 6; ++face)
  {
    const int col = face % 3;
    const int row = face / 3; // shader row 0 = uv.y < 0.5 = bottom half
    got[std::size_t(face)]
        = img.at(col * cw + cw / 2, img.height - 1 - (row * ch + ch / 2));
  }

  // (a) IDENTITY: cell f carries the colour predicted for face f. This is the
  // assertion that pins which face is which; the negative control in the header
  // reddens exactly cells 0 and 1 here.
  for(int face = 0; face < 6; ++face)
  {
    const auto want = predictedColour(face);
    INFO(
        "face " << kFaceNames[face] << " (" << face << "): got ("
                << int(got[std::size_t(face)][0]) << ","
                << int(got[std::size_t(face)][1]) << ","
                << int(got[std::size_t(face)][2]) << ") expected ("
                << int(want[0]) << "," << int(want[1]) << "," << int(want[2])
                << ")");
    CHECK(near(got[std::size_t(face)], want, kTol));
  }

  // (b) CARDINALITY: each predicted colour appears exactly once. Catches a
  // dropped or duplicated face (one camera packed six times, one layer blitted
  // into every face, a 240-byte camera binding under a 1440-byte block). It is
  // deliberately NOT a substitute for (a): a permutation leaves it green.
  for(int c = 0; c < 6; ++c)
  {
    const auto want = predictedColour(c);
    int found = 0;
    for(int face = 0; face < 6; ++face)
      if(near(got[std::size_t(face)], want, kTol))
        ++found;
    INFO(
        "colour for " << kFaceNames[c] << " found " << found
                      << " time(s) across the six probes");
    CHECK(found == 1);
  }
}
