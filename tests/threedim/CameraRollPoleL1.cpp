// Camera orientation: the Roll control turns the view around its axis, and
// looking straight down / up yields a well-defined orientation that is the
// limit of the nearby ones (QQuaternion::fromDirection snapped to a
// shortest-arc rotation within ~0.18 degrees of the pole).

#include <Threedim/Camera.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace
{
struct Frame
{
  QVector3D right, up, back;
};

Frame frameOf(const QQuaternion& q)
{
  return {
      q.rotatedVector({1.f, 0.f, 0.f}), q.rotatedVector({0.f, 1.f, 0.f}),
      q.rotatedVector({0.f, 0.f, 1.f})};
}

bool near(const QVector3D& a, const QVector3D& b, float eps = 1e-3f)
{
  return (a - b).length() < eps;
}

QQuaternion emitted(const Threedim::Camera& cam)
{
  REQUIRE(cam.m_state);
  REQUIRE(cam.m_state->roots);
  REQUIRE(cam.m_state->roots->size() == 1);
  const auto& node = (*cam.m_state->roots)[0];
  REQUIRE(node->children);
  for(const auto& c : *node->children)
    if(auto* x = ossia::get_if<ossia::scene_transform>(&c))
      return QQuaternion(x->rotation[3], x->rotation[0], x->rotation[1], x->rotation[2]);
  FAIL("no scene_transform");
  return {};
}

void place(Threedim::Camera& cam, QVector3D eye, QVector3D target, float roll)
{
  cam.inputs.eye.value = {eye.x(), eye.y(), eye.z()};
  cam.inputs.target.value = {target.x(), target.y(), target.z()};
  cam.inputs.roll.value = roll;
  cam.rebuild();
}
}

TEST_CASE("Camera without roll matches lookAt", "[threedim][camera][l1]")
{
  Threedim::Camera cam;
  place(cam, {1.f, 2.f, 5.f}, {0.f, 0.f, 0.f}, 0.f);
  QMatrix4x4 world;
  world.translate(1.f, 2.f, 5.f);
  world.rotate(emitted(cam));
  QMatrix4x4 view;
  view.lookAt({1.f, 2.f, 5.f}, {0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
  const QMatrix4x4 inv = world.inverted();
  for(int i = 0; i < 16; i++)
    CHECK(std::abs(inv.constData()[i] - view.constData()[i]) < 1e-4f);
}

TEST_CASE("Camera roll turns the view around its axis", "[threedim][camera][l1]")
{
  Threedim::Camera cam;
  place(cam, {0.f, 0.f, 5.f}, {0.f, 0.f, 0.f}, 90.f);
  const auto f = frameOf(emitted(cam));
  CHECK(near(f.back, {0.f, 0.f, 1.f}));
  CHECK(near(f.up, {-1.f, 0.f, 0.f}));
  CHECK(near(f.right, {0.f, 1.f, 0.f}));

  place(cam, {0.f, 0.f, 5.f}, {0.f, 0.f, 0.f}, -30.f);
  const auto g = frameOf(emitted(cam));
  CHECK(near(g.back, {0.f, 0.f, 1.f}));
  CHECK(near(g.up, {std::sin(0.5236f), std::cos(0.5236f), 0.f}));

  CHECK(near(frameOf(cam.orientation()).up, g.up));
}

TEST_CASE("Camera looking straight down or up does not snap", "[threedim][camera][l1]")
{
  Threedim::Camera cam;

  SECTION("down, approached from +Z")
  {
    place(cam, {0.f, 5.f, 0.f}, {0.f, 0.f, 0.f}, 0.f);
    const auto pole = frameOf(emitted(cam));
    CHECK(near(pole.back, {0.f, 1.f, 0.f}));
    CHECK(near(pole.up, {0.f, 0.f, -1.f}));
    CHECK(near(pole.right, {1.f, 0.f, 0.f}));
    for(float d : {0.5f, 0.05f, 0.005f, 0.0005f})
    {
      place(cam, {0.f, 5.f, d}, {0.f, 0.f, 0.f}, 0.f);
      const auto f = frameOf(emitted(cam));
      CHECK(near(f.right, pole.right, 0.2f));
      CHECK(near(f.up, pole.up, 0.2f));
    }
  }

  SECTION("up, approached from +Z")
  {
    place(cam, {0.f, -5.f, 0.f}, {0.f, 0.f, 0.f}, 0.f);
    const auto pole = frameOf(emitted(cam));
    CHECK(near(pole.back, {0.f, -1.f, 0.f}));
    CHECK(near(pole.up, {0.f, 0.f, 1.f}));
    place(cam, {0.f, -5.f, 0.0005f}, {0.f, 0.f, 0.f}, 0.f);
    CHECK(near(frameOf(emitted(cam)).up, pole.up, 0.01f));
  }

  SECTION("near the pole from +X the view keeps following lookAt")
  {
    for(float d : {0.05f, 0.01f, 0.005f, 0.001f})
    {
      place(cam, {d, 5.f, 0.f}, {0.f, 0.f, 0.f}, 0.f);
      const auto f = frameOf(emitted(cam));
      CHECK(near(f.up, {-1.f, 0.f, 0.f}, 0.01f));
      CHECK(near(f.right, {0.f, 0.f, -1.f}, 0.01f));
    }
  }

  SECTION("roll applies at the pole")
  {
    place(cam, {0.f, 5.f, 0.f}, {0.f, 0.f, 0.f}, 90.f);
    const auto f = frameOf(emitted(cam));
    CHECK(near(f.back, {0.f, 1.f, 0.f}));
    CHECK(near(f.up, {-1.f, 0.f, 0.f}));
  }

  SECTION("eye on target stays finite")
  {
    place(cam, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, 0.f);
    const auto q = emitted(cam);
    CHECK(std::abs(q.length() - 1.f) < 1e-4f);
  }
}
