// Threedim::CameraArray face orientations against the cubemap face layout
// (agent L4).
//
// Each face camera's right and up axes must be the face's sc and tc axes from
// the cube map face selection table (OpenGL 4.6 section 8.13, Vulkan "Cube Map
// Face Selection"), so a face rendered with this camera lands in the cube face
// the way a samplerCube lookup reads it back. The +Y and -Y faces look along
// the world up axis: their up vector is +Z / -Z, not the +Y reference that
// QQuaternion::fromDirection cannot use there.

#include <Threedim/CameraArray.hpp>

#include <ossia/detail/variant.hpp>

#include <QQuaternion>
#include <QVector3D>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;

namespace
{
struct CubeFace
{
  QVector3D major;
  QVector3D sc;
  QVector3D tc;
};

const CubeFace kCubeFaces[6]{
    {{1.f, 0.f, 0.f}, {0.f, 0.f, -1.f}, {0.f, -1.f, 0.f}},
    {{-1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, {0.f, -1.f, 0.f}},
    {{0.f, 1.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}},
    {{0.f, -1.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 0.f, -1.f}},
    {{0.f, 0.f, 1.f}, {1.f, 0.f, 0.f}, {0.f, -1.f, 0.f}},
    {{0.f, 0.f, -1.f}, {-1.f, 0.f, 0.f}, {0.f, -1.f, 0.f}},
};

QQuaternion rotation_of(const ossia::scene_node& n)
{
  REQUIRE(n.children);
  for(const auto& c : *n.children)
    if(auto* t = ossia::get_if<ossia::scene_transform>(&c))
      return QQuaternion(t->rotation[3], t->rotation[0], t->rotation[1], t->rotation[2]);
  FAIL("node carries no scene_transform");
  return {};
}

void checkAxis(const QVector3D& got, const QVector3D& want)
{
  INFO(
      "got (" << got.x() << ", " << got.y() << ", " << got.z() << ") want ("
              << want.x() << ", " << want.y() << ", " << want.z() << ")");
  CHECK(got.x() == Approx(want.x()).margin(1e-5));
  CHECK(got.y() == Approx(want.y()).margin(1e-5));
  CHECK(got.z() == Approx(want.z()).margin(1e-5));
}
}

TEST_CASE(
    "CameraArray face cameras span each cube face's sc / tc axes",
    "[threedim][camera][cubemap][l4]")
{
  Threedim::CameraArray n;
  n.inputs.origin.value = {3.f, -2.f, 7.f};
  n.rebuild();
  n();

  const auto& roots = *n.outputs.scene_out.scene.state->roots;
  REQUIRE(roots.size() == 6);
  for(int i = 0; i < 6; ++i)
  {
    INFO("face " << i);
    const QQuaternion q = rotation_of(*roots[std::size_t(i)]);
    CHECK(q.length() == Approx(1.f).margin(1e-5));
    const auto& face = kCubeFaces[i];
    checkAxis(q.rotatedVector({0.f, 0.f, -1.f}), face.major);
    checkAxis(q.rotatedVector({1.f, 0.f, 0.f}), face.sc);
    checkAxis(q.rotatedVector({0.f, 1.f, 0.f}), face.tc);
  }
}
