// Threedim::CameraArray (52104d297f) — the producer side of issue #163.
//
// CameraArray claims to emit the six GL-ordered cubemap faces at 90° FoV that
// a MULTIVIEW=6 shader indexes through gl_ViewIndex. Nothing anywhere asserted
// that claim: not the face count, not the ids merge_scenes needs to be
// distinct, and not the orientations — which is the one thing in the file that
// is easy to get backwards, because QQuaternion::fromDirection maps local +Z
// to its argument while a GL camera looks along local -Z, so the code passes
// the NEGATED forward vector.
//
// rebuild() / operator()() are entirely header-inline and GPU-free; the
// out-of-line init/update/release are never called here, so both arena slot
// refs stay invalid and the render-thread paths are not entered.

#include <Threedim/CameraArray.hpp>

#include <ossia/detail/variant.hpp>

#include <QVector3D>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <set>

using Catch::Approx;

namespace
{
//! Order and direction of the six faces, GL cubemap convention.
constexpr float kFaceForward[6][3]{
    {1.f, 0.f, 0.f},  {-1.f, 0.f, 0.f}, {0.f, 1.f, 0.f},
    {0.f, -1.f, 0.f}, {0.f, 0.f, 1.f},  {0.f, 0.f, -1.f}};

const ossia::scene_transform& xform_of(const ossia::scene_node& n)
{
  REQUIRE(n.children);
  for(const auto& c : *n.children)
    if(auto* t = ossia::get_if<ossia::scene_transform>(&c))
      return *t;
  FAIL("node carries no scene_transform");
  static const ossia::scene_transform dummy{};
  return dummy;
}

const ossia::camera_component& camera_of(const ossia::scene_node& n)
{
  REQUIRE(n.children);
  for(const auto& c : *n.children)
    if(auto* p = ossia::get_if<ossia::camera_component_ptr>(&c))
      if(*p)
        return **p;
  FAIL("node carries no camera_component");
  static const ossia::camera_component dummy{};
  return dummy;
}
} // namespace

TEST_CASE("CameraArray emits six distinct camera nodes", "[threedim][camera][163]")
{
  Threedim::CameraArray n;
  n.inputs.origin.value = {1.f, 2.f, 3.f};
  n.inputs.near_plane.value = 0.5f;
  n.inputs.far_plane.value = 100.f;

  n();
  const auto& st = n.outputs.scene_out.scene.state;
  REQUIRE(st);
  REQUIRE(st->roots);
  REQUIRE(st->roots->size() == 6);

  std::set<uint64_t> ids;
  for(const auto& node : *st->roots)
  {
    REQUIRE(node);
    // merge_scenes collapses same-id camera entries, so every face needs a
    // stable, distinct, non-zero id.
    CHECK(node->id.value != 0);
    ids.insert(node->id.value);
    REQUIRE(node->children);
    CHECK(node->children->size() == 2);
  }
  CHECK(ids.size() == 6);
}

TEST_CASE("CameraArray gives every face a square 90-degree frustum",
          "[threedim][camera][163]")
{
  Threedim::CameraArray n;
  n.inputs.near_plane.value = 0.25f;
  n.inputs.far_plane.value = 512.f;
  n();

  const auto& roots = *n.outputs.scene_out.scene.state->roots;
  for(const auto& node : roots)
  {
    const auto& cam = camera_of(*node);
    CHECK(cam.projection == ossia::camera_projection::perspective);
    CHECK(cam.yfov == Approx(M_PI / 2.0));
    CHECK(cam.aspect_ratio == Approx(1.f));
    CHECK(cam.znear == Approx(0.25f));
    CHECK(cam.zfar == Approx(512.f));
  }
}

TEST_CASE("CameraArray places every face at the origin control, unscaled",
          "[threedim][camera][163]")
{
  Threedim::CameraArray n;
  n.inputs.origin.value = {-4.f, 7.5f, 0.25f};
  n();

  for(const auto& node : *n.outputs.scene_out.scene.state->roots)
  {
    const auto& t = xform_of(*node);
    CHECK(t.translation[0] == Approx(-4.f));
    CHECK(t.translation[1] == Approx(7.5f));
    CHECK(t.translation[2] == Approx(0.25f));
    CHECK(t.scale[0] == Approx(1.f));
    CHECK(t.scale[1] == Approx(1.f));
    CHECK(t.scale[2] == Approx(1.f));
  }
}

TEST_CASE("CameraArray face rotations point local -Z at +X,-X,+Y,-Y,+Z,-Z",
          "[threedim][camera][163]")
{
  Threedim::CameraArray n;
  n();

  const auto& roots = *n.outputs.scene_out.scene.state->roots;
  REQUIRE(roots.size() == 6);
  for(int i = 0; i < 6; ++i)
  {
    const auto& t = xform_of(*roots[std::size_t(i)]);
    const QQuaternion q(t.rotation[3], t.rotation[0], t.rotation[1], t.rotation[2]);
    CHECK(q.length() == Approx(1.f).margin(1e-4));

    const QVector3D looked = q.rotatedVector(QVector3D(0.f, 0.f, -1.f));
    const QVector3D want(
        kFaceForward[i][0], kFaceForward[i][1], kFaceForward[i][2]);
    INFO("face " << i);
    CHECK(looked.x() == Approx(want.x()).margin(1e-4));
    CHECK(looked.y() == Approx(want.y()).margin(1e-4));
    CHECK(looked.z() == Approx(want.z()).margin(1e-4));
  }
}

TEST_CASE("CameraArray face 0 is the active camera", "[threedim][camera][163]")
{
  Threedim::CameraArray n;
  n();
  const auto& st = n.outputs.scene_out.scene.state;
  CHECK(st->active_camera_id.value == (*st->roots)[0]->id.value);
  CHECK(st->active_camera_id.value != 0);
}

TEST_CASE("CameraArray bumps version per rebuild and latches dirty once",
          "[threedim][camera][163]")
{
  Threedim::CameraArray n;

  n();
  const auto& st = n.outputs.scene_out.scene.state;
  REQUIRE(st);
  CHECK(st->version == 1);
  CHECK(n.outputs.scene_out.dirty == ossia::scene_port::dirty_transform);

  uint64_t ids_before[6]{};
  for(int i = 0; i < 6; ++i)
    ids_before[i] = (*st->roots)[std::size_t(i)]->id.value;
  const uint64_t active_before = st->active_camera_id.value;

  n();
  CHECK(n.outputs.scene_out.dirty == 0); // one-shot latch
  CHECK(n.outputs.scene_out.scene.state == st);

  n.inputs.origin.value = {9.f, 0.f, 0.f};
  n.rebuild(); // what the control's update() callback does
  CHECK(n.outputs.scene_out.scene.state->version == 2);
  n();
  CHECK(n.outputs.scene_out.dirty == ossia::scene_port::dirty_transform);

  // The ids are seeded once and must survive a rebuild, or downstream would
  // see six removals and six additions on every knob turn.
  const auto& roots = *n.outputs.scene_out.scene.state->roots;
  REQUIRE(roots.size() == 6);
  for(int i = 0; i < 6; ++i)
    CHECK(roots[std::size_t(i)]->id.value == ids_before[i]);
  CHECK(n.outputs.scene_out.scene.state->active_camera_id.value == active_before);
}

TEST_CASE("CameraArray leaves the raw slot refs alone without a RenderList",
          "[threedim][camera][163]")
{
  Threedim::CameraArray n;
  n();
  for(const auto& node : *n.outputs.scene_out.scene.state->roots)
  {
    CHECK_FALSE(xform_of(*node).raw_slot.valid());
    CHECK_FALSE(camera_of(*node).raw_slot.valid());
  }
}
