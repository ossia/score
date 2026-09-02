// A6 / P1-14 / G17 — an orthographic or fulldome camera must not render as a
// perspective one. FIXED; this file is now the regression guard.
//
// The defect (history)
// --------------------
// `ossia::camera_projection` carries perspective / orthographic / fulldome
// (3rdparty/libossia/src/ossia/dataflow/geometry_port.hpp:875, the field on
// camera_component at :879, with the orthographic extents xmag/ymag at
// :884-885). Real assets author it: GltfParser.cpp and FbxParser.cpp both emit
// camera_projection::orthographic. But `packCameraUBO`
// (src/plugins/score-plugin-gfx/Gfx/Graph/CameraMath.cpp) never read
// `cam.projection` and unconditionally built `setReverseZPerspective`, so an
// orthographic or fulldome camera was transported through the whole scene graph
// and rendered as a perspective one. The two cases below carried
// [!shouldfail]; they no longer do.
//
// The fix
// -------
//  * orthographic: packCameraUBO builds `setReverseZOrthographic` from
//    cam.xmag / cam.ymag (half-extents, per the glTF definition) and
//    cam.znear / cam.zfar, keeping the project-wide reverse-Z convention
//    (near -> +1, far -> -1 in NDC z, GREATER compare, clear-depth 0.0);
//  * every camera now also publishes WHICH projection it is, as a mode code in
//    `params[3]` — `camera.params.w` in a scene shader — using
//    score::gfx::CameraProjectionMode. Perspective is 0, which is the value
//    every camera got before this existed, so no shader in the corpus changes
//    behaviour (all 28 real Model Displays are Perspective).
//  * fulldome: an angular (fisheye) mapping is not expressible as a 4x4 — at
//    180 degrees the frustum is degenerate — so the packed matrix stays the
//    linear frustum that culling and the depth buffer work against, and
//    params.w = 2 is what tells the scene shader to apply the dome remap.
//    Before, the shader had no way to know at all.
//
// Why this is unit-level (option 1) and not a render-difference oracle
// -------------------------------------------------------------------
// The projection field's only consumers in the render path are the two
// packCameraUBO call sites in ScenePreprocessorNode.cpp (default camera,
// flattened scene cameras); no other code in score-plugin-gfx reads
// camera_component::projection. Every scene shader receives the camera
// exclusively through the 240-byte CameraUBOData those calls produce. So if two
// camera states differing only in `projection` packed to byte-identical UBOs —
// which they did — the rendered frames of two otherwise-identical scenes were
// byte-identical by construction. The memcmp on the packed UBO *is* the
// frame-difference oracle, evaluated at the exact boundary where the
// information used to be lost.
//
// Structure
// ---------
// One case per projection kind, plus two controls that must hold whatever
// packCameraUBO does: identical inputs pack identically (so a "they differ"
// assertion cannot be satisfied by nondeterminism), and a genuinely different
// parameter does change the bytes (so the comparator is not inert).
//
//   ctest -R gfx_camera_projection_pin --output-on-failure
// =============================================================================
#include <Gfx/Graph/CameraMath.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QMatrix4x4>
#include <QVector4D>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstring>

using namespace score::gfx;
using Catch::Approx;

namespace
{
// A camera identical in every metric parameter; only `projection` varies
// between the two sides of each comparison.
ossia::camera_component makeCamera(ossia::camera_projection proj)
{
  ossia::camera_component cam;
  cam.projection = proj;
  cam.yfov = 0.9f;
  cam.aspect_ratio = 16.f / 9.f;
  cam.xmag = 2.5f;
  cam.ymag = 2.5f;
  cam.znear = 0.1f;
  cam.zfar = 100.f;
  return cam;
}

// A non-trivial world transform: the eye off-origin and rotated, so the view
// matrix is not identity and a packing slip could not hide in zeros.
QMatrix4x4 makeWorld()
{
  QMatrix4x4 world;
  world.translate(1.f, 2.f, 5.f);
  world.rotate(30.f, 0.f, 1.f, 0.f);
  return world;
}

CameraUBOData pack(const ossia::camera_component& cam)
{
  CameraUBOData out;
  packCameraUBO(out, cam, makeWorld(), QSize{1280, 720}, 0.25f);
  return out;
}

bool sameBytes(const CameraUBOData& a, const CameraUBOData& b)
{
  return std::memcmp(&a, &b, sizeof(CameraUBOData)) == 0;
}

// Rebuild a QMatrix4x4 from the packed column-major floats (writeMat4 copies
// QMatrix4x4::constData(), which is column-major; QMatrix4x4::data() writes
// the same storage).
QMatrix4x4 unpackMat4(const float src[16])
{
  QMatrix4x4 m;
  std::memcpy(m.data(), src, 16 * sizeof(float));
  return m;
}

float ndcX(const QMatrix4x4& proj, float viewX, float viewY, float viewZ)
{
  const QVector4D clip = proj * QVector4D{viewX, viewY, viewZ, 1.f};
  return clip.x() / clip.w();
}
}

//------------------------------------------------------------------------------
// The two behaviours the defect destroyed. Red before the CameraMath fix.
//------------------------------------------------------------------------------

TEST_CASE(
    "an orthographic camera packs a different UBO than a perspective one",
    "[gfx][camera][projection][issueG17]")
{
  // Two cameras differing ONLY in the projection kind. Since the packed UBO
  // is the sole channel through which a camera reaches the scene shaders
  // (ScenePreprocessorNode.cpp:3677/:3688), identical bytes here mean two
  // otherwise-identical scenes render byte-identically — perspective-as-
  // orthographic. The correct behaviour asserted below is "they differ".
  const auto persp = pack(makeCamera(ossia::camera_projection::perspective));
  const auto ortho = pack(makeCamera(ossia::camera_projection::orthographic));

  // Primary oracle: the projection (and hence viewProjection) matrices must
  // differ. Today packCameraUBO ignores cam.projection (CameraMath.cpp:26-27)
  // and both are the same setReverseZPerspective matrix.
  CHECK(
      std::memcmp(persp.projection, ortho.projection, sizeof(persp.projection))
      != 0);
  CHECK(!sameBytes(persp, ortho));

  // Closed-form parallel-lines oracle: under any orthographic projection,
  // the projected x of a point is independent of its depth — parallel
  // depth-separated edges stay parallel and equally spaced. Under the
  // perspective matrix packCameraUBO currently emits, the far point pulls
  // toward the axis (foreshortening), so this fails today.
  const QMatrix4x4 orthoProj = unpackMat4(ortho.projection);
  const float xNear = ndcX(orthoProj, 1.f, 0.5f, -1.f);
  const float xFar = ndcX(orthoProj, 1.f, 0.5f, -50.f);
  CHECK(xNear == Approx(xFar).margin(1e-4));

  // It is a true parallel projection: no perspective divide at all.
  CHECK(orthoProj(3, 0) == Approx(0.f));
  CHECK(orthoProj(3, 1) == Approx(0.f));
  CHECK(orthoProj(3, 2) == Approx(0.f));
  CHECK(orthoProj(3, 3) == Approx(1.f));

  // The half-extents come from xmag/ymag: a point at x = xmag lands on the NDC
  // edge, not somewhere arbitrary.
  const auto cam = makeCamera(ossia::camera_projection::orthographic);
  CHECK(ndcX(orthoProj, cam.xmag, 0.f, -10.f) == Approx(1.f).margin(1e-4));

  // ...and it keeps the project-wide reverse-Z convention: near -> +1,
  // far -> -1 (CameraMath.hpp), so it shares the depth buffer with everything
  // else instead of rendering inside out.
  const auto ndcZ = [&](float z) {
    const QVector4D clip = orthoProj * QVector4D{0.f, 0.f, z, 1.f};
    return clip.z() / clip.w();
  };
  CHECK(ndcZ(-cam.znear) == Approx(1.f).margin(1e-4));
  CHECK(ndcZ(-cam.zfar) == Approx(-1.f).margin(1e-4));

  // And the mode is published to the shader.
  CHECK(persp.params[3] == Approx(float(int(CameraProjectionMode::Perspective))));
  CHECK(ortho.params[3] == Approx(float(int(CameraProjectionMode::Orthographic))));
}

TEST_CASE(
    "a fulldome camera packs a different UBO than a perspective one",
    "[gfx][camera][projection][issueG17]")
{
  // Fulldome is a shader-side angular mapping rather than a linear matrix, so
  // this case requires that SOME of the 240 packed bytes differ — any strategy
  // that lets a shader distinguish the two satisfies it. Before the fix none
  // did: packCameraUBO never read cam.projection, so a dome camera was
  // indistinguishable from a perspective one by the time it reached a shader.
  const auto persp = pack(makeCamera(ossia::camera_projection::perspective));
  const auto dome = pack(makeCamera(ossia::camera_projection::fulldome));

  CHECK(!sameBytes(persp, dome));

  // Concretely: the mode code a dome shader branches on.
  CHECK(dome.params[3] == Approx(float(int(CameraProjectionMode::Fulldome))));
  CHECK(dome.params[3] != Approx(persp.params[3]));
}

//------------------------------------------------------------------------------
// Controls — these held before the fix too, and must keep holding.
//------------------------------------------------------------------------------

TEST_CASE(
    "two cameras with the same projection pack byte-identical UBOs",
    "[gfx][camera][projection][issueG17]")
{
  // Determinism control: the shouldfail cases above assert a difference, so
  // they must not be satisfiable by nondeterministic packing (uninitialized
  // padding, time-dependent state). Identical inputs -> identical 240 bytes.
  const auto a = pack(makeCamera(ossia::camera_projection::perspective));
  const auto b = pack(makeCamera(ossia::camera_projection::perspective));
  CHECK(sameBytes(a, b));

  const auto c = pack(makeCamera(ossia::camera_projection::orthographic));
  const auto d = pack(makeCamera(ossia::camera_projection::orthographic));
  CHECK(sameBytes(c, d));
}

TEST_CASE(
    "a genuinely different camera parameter does change the packed UBO",
    "[gfx][camera][projection][issueG17]")
{
  // Comparator-sanity control: prove the memcmp oracle CAN see a projection
  // change, so the shouldfail cases are red because packCameraUBO drops the
  // information, not because the comparison is inert.
  auto wide = makeCamera(ossia::camera_projection::perspective);
  auto narrow = makeCamera(ossia::camera_projection::perspective);
  narrow.yfov = 0.4f;

  const auto a = pack(wide);
  const auto b = pack(narrow);
  CHECK(
      std::memcmp(a.projection, b.projection, sizeof(a.projection)) != 0);
  CHECK(!sameBytes(a, b));

  // And a perspective-behaviour witness: the perspective matrix really does
  // foreshorten, so the parallel-lines closed form above is discriminating.
  const QMatrix4x4 perspProj = unpackMat4(a.projection);
  const float xNear = ndcX(perspProj, 1.f, 0.5f, -1.f);
  const float xFar = ndcX(perspProj, 1.f, 0.5f, -50.f);
  CHECK(std::abs(xNear - xFar) > 1e-3f);
}
