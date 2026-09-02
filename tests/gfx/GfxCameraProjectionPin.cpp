// P1-14 / G17 — `an orthographic camera does not render as a perspective one`
// EXPECTED RED: the defect cases below carry [!shouldfail].
//
// The defect
// ----------
// `ossia::camera_projection` carries perspective / orthographic / fulldome
// (3rdparty/libossia/src/ossia/dataflow/geometry_port.hpp:875, the field on
// camera_component at :879, with the orthographic extents xmag/ymag at
// :884-885). Real assets author it: GltfParser.cpp:468 and FbxParser.cpp:730
// both emit camera_projection::orthographic. But `packCameraUBO`
// (src/plugins/score-plugin-gfx/Gfx/Graph/CameraMath.cpp:8-46) never reads
// `cam.projection` and unconditionally builds `setReverseZPerspective`
// (CameraMath.cpp:26-27). An orthographic or fulldome camera is transported
// through the whole scene graph and rendered as a perspective one.
//
// Why this pin is unit-level (option 1) and not a render-difference oracle
// -----------------------------------------------------------------------
// The projection field's only possible consumers in the render path are the
// two packCameraUBO call sites in ScenePreprocessorNode.cpp (:3677 default
// camera, :3688 flattened scene cameras); no other code in score-plugin-gfx
// reads camera_component::projection (verified by grep across the plugin).
// Every scene shader receives the camera exclusively through the 240-byte
// CameraUBOData those calls produce. Therefore: if two camera states
// differing only in `projection` pack to byte-identical UBOs — which this
// test proves they do today — the rendered frames of two otherwise-identical
// scenes are byte-identical by construction. A GPU render-difference oracle
// would add hardware dependence without adding discrimination; the memcmp on
// the packed UBO *is* the frame-difference oracle, evaluated at the exact
// boundary where the information is lost. (The existing GfxPipeline-style
// fixtures build ISFNode graphs directly and do not route an ossia camera
// with a projection field into ScenePreprocessor without substantial new
// plumbing; nothing a render leg could observe is not already decided here.)
//
// Structure
// ---------
// [!shouldfail] applies per TEST_CASE, so:
//   - one shouldfail case per defective projection (orthographic, fulldome),
//     so a partial fix (e.g. ortho branch only) flips exactly the case it
//     fixes and CI flags the flip instead of hiding it;
//   - the control cases (same projection -> identical bytes; a genuinely
//     different parameter -> different bytes) are separate, NON-shouldfail
//     TEST_CASEs, so the pin cannot "pass" through a broken comparator or a
//     nondeterministic pack.
//
// Proposed fix direction (recorded per spec as a proposal, not a change)
// ---------------------------------------------------------------------
// In packCameraUBO, switch on cam.projection:
//   - orthographic: build a reverse-Z orthographic matrix from
//     cam.xmag/cam.ymag (half-extents, aspect-corrected like the perspective
//     branch) and cam.znear/cam.zfar, keeping the project-wide reverse-Z
//     convention documented in CameraMath.hpp (near -> +1, far -> -1 NDC,
//     GREATER depth compare, clear-depth 0.0);
//   - fulldome: either a dedicated matrix or a flag in out.params[3]
//     (currently always 0.f) consumed by the scene shaders — either strategy
//     changes at least one of the 240 packed bytes, which is all the
//     fulldome case below asserts.
// Fixing packCameraUBO this way flips the two shouldfail cases green.
//
// Registration: score_add_gfx_test(camera_projection_pin GfxCameraProjectionPin.cpp)
// packCameraUBO is SCORE_PLUGIN_GFX_EXPORT (CameraMath.hpp), so linking
// score_plugin_gfx (which score_add_gfx_test already does) is sufficient —
// no score_plugin_hidden_sources / direct compilation of CameraMath.cpp is
// needed.

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
// Defect pins — expected red today, [!shouldfail].
//------------------------------------------------------------------------------

TEST_CASE(
    "an orthographic camera packs a different UBO than a perspective one",
    "[gfx][camera][projection][issueG17][!shouldfail]")
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
}

TEST_CASE(
    "a fulldome camera packs a different UBO than a perspective one",
    "[gfx][camera][projection][issueG17][!shouldfail]")
{
  // Fulldome may legitimately be implemented as a shader-side mapping rather
  // than a linear matrix, so this case only requires that SOME of the 240
  // packed bytes differ (e.g. a mode flag in params[3], currently always
  // 0.f) — any implementation strategy that lets a shader distinguish the
  // two satisfies it. Today none do: packCameraUBO never reads
  // cam.projection.
  const auto persp = pack(makeCamera(ossia::camera_projection::perspective));
  const auto dome = pack(makeCamera(ossia::camera_projection::fulldome));

  CHECK(!sameBytes(persp, dome));
}

//------------------------------------------------------------------------------
// Controls — must pass today and after the fix; deliberately NOT shouldfail,
// in their own TEST_CASEs since [!shouldfail] applies per TEST_CASE.
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
