// Threedim::ShadowCascadeSetup — the pure QMatrix4x4 cascade math f92f98ceab
// reworked (findActiveCamera, the reverse-Z NDC flip, and the corrected zPad
// direction) with no coverage of any kind.
//
// Everything here goes through the public rebuild()/operator()() and reads
// ossia::shadow_cascades_info off the emitted scene_state. No GPU.
//
// The depth-convention assertions are derived from the code's own geometry,
// not fitted to its output:
//   lightView.lookAt(centroid - lightDir, centroid, up) puts the eye upstream
//   of the light's travel, so world +lightDir maps to light-view -Z and points
//   TOWARD the light source get the larger light-view z (maxLS). ortho() is
//   handed nearPlane = -maxLS.z(), so before the flip toward-light maps to
//   NDC -1; the zFlip(2,2) = -1 turns that into +1 — the project's reverse-Z
//   convention (near -> +1, depth op Greater, clear 0). The zPad then extends
//   maxLS, i.e. the toward-light side, so an occluder between the light and
//   the slice stays inside the frustum while one beyond the slice does not.

#include <Threedim/ShadowCascadeSetup.hpp>

#include <ossia/detail/variant.hpp>

#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>
#include <QVector4D>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

using Catch::Approx;

namespace
{
ossia::scene_transform trs(float tx, float ty, float tz)
{
  ossia::scene_transform t;
  t.translation[0] = tx;
  t.translation[1] = ty;
  t.translation[2] = tz;
  t.rotation[3] = 1.f;
  t.scale[0] = t.scale[1] = t.scale[2] = 1.f;
  return t;
}

ossia::scene_node_ptr camera_node(
    uint64_t id, float tx, float znear = 0.1f, float zfar = 1000.f)
{
  auto cam = std::make_shared<ossia::camera_component>();
  cam->projection = ossia::camera_projection::perspective;
  cam->yfov = float(M_PI) / 4.f;
  cam->aspect_ratio = 16.f / 9.f;
  cam->znear = znear;
  cam->zfar = zfar;

  auto kids = std::make_shared<std::vector<ossia::scene_payload>>();
  kids->push_back(trs(tx, 0.f, 0.f));
  kids->push_back(ossia::camera_component_ptr(std::move(cam)));

  auto n = std::make_shared<ossia::scene_node>();
  n->id.value = id;
  n->children = std::move(kids);
  return n;
}

//! A directional light whose local -Z, rotated by `q`, is the world direction.
ossia::scene_node_ptr light_node(uint64_t id, const QQuaternion& q)
{
  auto light = std::make_shared<ossia::light_component>();
  light->type = ossia::light_type::directional;

  ossia::scene_transform t;
  t.rotation[0] = q.x();
  t.rotation[1] = q.y();
  t.rotation[2] = q.z();
  t.rotation[3] = q.scalar();
  t.scale[0] = t.scale[1] = t.scale[2] = 1.f;

  auto kids = std::make_shared<std::vector<ossia::scene_payload>>();
  kids->push_back(t);
  kids->push_back(ossia::light_component_ptr(std::move(light)));

  auto n = std::make_shared<ossia::scene_node>();
  n->id.value = id;
  n->children = std::move(kids);
  return n;
}

std::shared_ptr<ossia::scene_state>
make_state(std::vector<ossia::scene_node_ptr> roots, uint64_t active_cam = 0)
{
  auto s = std::make_shared<ossia::scene_state>();
  s->roots
      = std::make_shared<std::vector<ossia::scene_node_ptr>>(std::move(roots));
  s->active_camera_id.value = active_cam;
  s->version = 1;
  return s;
}

const ossia::shadow_cascades_info& run(Threedim::ShadowCascadeSetup& n)
{
  n.rebuild();
  n();
  REQUIRE(n.outputs.scene_out.scene.state);
  return n.outputs.scene_out.scene.state->shadow_cascades;
}

QMatrix4x4 mat_of(const ossia::shadow_cascades_info& i, int cascade)
{
  return QMatrix4x4{
      QMatrix4x4{i.light_view_proj[cascade]}.transposed()}; // constData is column-major
}

//! NDC z of a world point through a cascade's light view-projection.
float ndc_z(const QMatrix4x4& m, const QVector3D& p)
{
  const QVector4D c = m * QVector4D(p, 1.f);
  return c.w() != 0.f ? c.z() / c.w() : 0.f;
}
} // namespace

TEST_CASE("ShadowCascadeSetup: split depths follow the practical scheme",
          "[threedim][shadow]")
{
  Threedim::ShadowCascadeSetup n;
  n.inputs.scene_in.scene.state = make_state({camera_node(1, 0.f)});
  n.inputs.cascade_count.value = 4;
  n.inputs.camera_near.value = 1.f;
  n.inputs.camera_far.value = 1000.f;
  n.inputs.shadow_distance.value = 100.f;
  n.inputs.light_direction.value = {0.f, -1.f, 0.f};

  const float nearZ = 1.f;
  const float farZ = 100.f; // min(camera_far, shadow_distance)

  SECTION("lambda = 0 is uniform")
  {
    n.inputs.lambda.value = 0.f;
    const auto& i = run(n);
    REQUIRE(i.cascade_count == 4u);
    CHECK(i.split_view_depths[0] == Approx(nearZ));
    CHECK(i.split_view_depths[4] == Approx(farZ));
    for(int k = 1; k < 4; ++k)
    {
      const float p = float(k) / 4.f;
      CHECK(i.split_view_depths[k] == Approx(nearZ + (farZ - nearZ) * p));
    }
  }

  SECTION("lambda = 1 is logarithmic")
  {
    n.inputs.lambda.value = 1.f;
    const auto& i = run(n);
    for(int k = 1; k < 4; ++k)
    {
      const float p = float(k) / 4.f;
      CHECK(i.split_view_depths[k]
            == Approx(nearZ * std::pow(farZ / nearZ, p)).epsilon(1e-4));
    }
  }

  SECTION("lambda = 0.5 is the mean of the two, and splits stay monotone")
  {
    n.inputs.lambda.value = 0.5f;
    const auto& i = run(n);
    for(int k = 1; k < 4; ++k)
    {
      const float p = float(k) / 4.f;
      const float lg = nearZ * std::pow(farZ / nearZ, p);
      const float un = nearZ + (farZ - nearZ) * p;
      CHECK(i.split_view_depths[k] == Approx(0.5f * lg + 0.5f * un).epsilon(1e-4));
    }
    for(int k = 0; k < 4; ++k)
      CHECK(i.split_view_depths[k] < i.split_view_depths[k + 1]);
  }

  SECTION("shadow_distance shortens the range; camera_far does not lengthen it")
  {
    n.inputs.lambda.value = 0.f;
    n.inputs.shadow_distance.value = 30.f;
    const auto& i = run(n);
    CHECK(i.split_view_depths[4] == Approx(30.f));
    CHECK(i.shadow_distance == Approx(30.f));
  }
}

TEST_CASE("ShadowCascadeSetup: cascade count is clamped to 1..8",
          "[threedim][shadow]")
{
  Threedim::ShadowCascadeSetup n;
  n.inputs.scene_in.scene.state = make_state({camera_node(1, 0.f)});
  n.inputs.light_direction.value = {0.f, -1.f, 0.f};

  n.inputs.cascade_count.value = 0;
  CHECK(run(n).cascade_count == 1u);
  n.inputs.cascade_count.value = -5;
  CHECK(run(n).cascade_count == 1u);
  n.inputs.cascade_count.value = 99;
  CHECK(run(n).cascade_count == 8u);
  n.inputs.cascade_count.value = 3;
  CHECK(run(n).cascade_count == 3u);
}

TEST_CASE("ShadowCascadeSetup: the light direction control overrides the scene",
          "[threedim][shadow]")
{
  Threedim::ShadowCascadeSetup n;

  SECTION("a non-zero control wins and is normalised")
  {
    n.inputs.scene_in.scene.state = make_state({camera_node(1, 0.f)});
    n.inputs.light_direction.value = {0.f, -3.f, 0.f};
    const auto& i = run(n);
    CHECK(i.light_direction[0] == Approx(0.f));
    CHECK(i.light_direction[1] == Approx(-1.f));
    CHECK(i.light_direction[2] == Approx(0.f));
  }

  SECTION("a zero control falls back to the first directional light in the tree")
  {
    // Local -Z rotated to point along world +X.
    const auto q = QQuaternion::rotationTo(
        QVector3D(0, 0, -1), QVector3D(1, 0, 0));
    n.inputs.scene_in.scene.state
        = make_state({camera_node(1, 0.f), light_node(2, q)});
    n.inputs.light_direction.value = {0.f, 0.f, 0.f};
    const auto& i = run(n);
    CHECK(i.light_direction[0] == Approx(1.f).margin(1e-4));
    CHECK(i.light_direction[1] == Approx(0.f).margin(1e-4));
    CHECK(i.light_direction[2] == Approx(0.f).margin(1e-4));
  }

  SECTION("a zero control with no light in the tree uses the built-in default")
  {
    n.inputs.scene_in.scene.state = make_state({camera_node(1, 0.f)});
    n.inputs.light_direction.value = {0.f, 0.f, 0.f};
    const auto& i = run(n);
    const QVector3D d
        = QVector3D(-0.4f, -0.8f, -0.6f).normalized();
    CHECK(i.light_direction[0] == Approx(d.x()).margin(1e-4));
    CHECK(i.light_direction[1] == Approx(d.y()).margin(1e-4));
    CHECK(i.light_direction[2] == Approx(d.z()).margin(1e-4));
  }
}

TEST_CASE("ShadowCascadeSetup: active_camera_id selects which camera is fitted",
          "[threedim][shadow]")
{
  // Two cameras 200 units apart on X: the cascades fitted to one cannot equal
  // the cascades fitted to the other.
  auto camA = camera_node(11, 0.f);
  auto camB = camera_node(22, 200.f);

  const auto fit = [&](uint64_t active) {
    Threedim::ShadowCascadeSetup n;
    n.inputs.scene_in.scene.state = make_state({camA, camB}, active);
    n.inputs.cascade_count.value = 2;
    n.inputs.camera_near.value = 1.f;
    n.inputs.camera_far.value = 200.f;
    n.inputs.shadow_distance.value = 200.f;
    n.inputs.lambda.value = 0.5f;
    n.inputs.light_direction.value = {0.f, -1.f, 0.f};
    n.rebuild();
    n();
    const auto& i = n.outputs.scene_out.scene.state->shadow_cascades;
    return std::vector<float>(
        i.light_view_proj[0], i.light_view_proj[0] + 16);
  };

  const auto first = fit(0);   // no id filter: first camera encountered wins
  const auto pinA = fit(11);
  const auto pinB = fit(22);

  // Pinning the first camera must reproduce the unfiltered fit...
  for(int k = 0; k < 16; ++k)
    CHECK(pinA[k] == Approx(first[k]).margin(1e-4));

  // ...and pinning the second must not: the id filter has to skip camA.
  bool differs = false;
  for(int k = 0; k < 16; ++k)
    if(std::abs(pinB[k] - first[k]) > 1e-3f)
      differs = true;
  CHECK(differs);
}

TEST_CASE("ShadowCascadeSetup: the cascade matrix is reverse-Z",
          "[threedim][shadow]")
{
  Threedim::ShadowCascadeSetup n;
  n.inputs.scene_in.scene.state = make_state({camera_node(1, 0.f)});
  n.inputs.cascade_count.value = 1;
  n.inputs.camera_near.value = 1.f;
  n.inputs.camera_far.value = 100.f;
  n.inputs.shadow_distance.value = 100.f;
  n.inputs.lambda.value = 0.f;
  // Light travelling straight down: the light source is above, at +Y.
  n.inputs.light_direction.value = {0.f, -1.f, 0.f};

  const auto& info = run(n);
  const QMatrix4x4 m = mat_of(info, 0);

  // A point inside the slice, roughly at the centre of the fitted volume.
  const QVector3D centre(0.f, 0.f, -50.f);
  const float base = ndc_z(m, centre);

  // Toward the light source is -lightDir = +Y. Reverse-Z: near -> +1.
  CHECK(ndc_z(m, centre + QVector3D(0.f, 5.f, 0.f)) > base);
  CHECK(ndc_z(m, centre - QVector3D(0.f, 5.f, 0.f)) < base);
}

TEST_CASE("ShadowCascadeSetup: the depth pad extends toward the light",
          "[threedim][shadow]")
{
  Threedim::ShadowCascadeSetup n;
  n.inputs.scene_in.scene.state = make_state({camera_node(1, 0.f)});
  n.inputs.cascade_count.value = 1;
  n.inputs.camera_near.value = 1.f;
  n.inputs.camera_far.value = 100.f;
  n.inputs.shadow_distance.value = 100.f;
  n.inputs.lambda.value = 0.f;
  n.inputs.light_direction.value = {0.f, -1.f, 0.f};

  const auto& info = run(n);
  const QMatrix4x4 m = mat_of(info, 0);
  const QVector3D centre(0.f, 0.f, -50.f);

  // How far the frustum reaches on each side of the slice along the light
  // axis, measured by walking until NDC z leaves [-1, 1].
  const auto reach = [&](float sign) {
    float h = 0.f;
    for(float step = 0.5f; h < 5000.f; h += step)
      if(std::abs(ndc_z(m, centre + QVector3D(0.f, sign * h, 0.f))) > 1.f)
        break;
    return h;
  };

  const float toward_light = reach(+1.f);
  const float away = reach(-1.f);
  INFO("toward light " << toward_light << " / away " << away);
  // An occluder between the light and the slice must be inside the frustum;
  // padding minLS instead (the pre-f92f98ceab direction) would only add room
  // beyond the slice, where nothing can cast into it.
  CHECK(toward_light > away);
}

TEST_CASE("ShadowCascadeSetup: passthrough and caching", "[threedim][shadow]")
{
  Threedim::ShadowCascadeSetup n;
  n.inputs.light_direction.value = {0.f, -1.f, 0.f};

  SECTION("a null upstream is forwarded as null")
  {
    n();
    CHECK(n.outputs.scene_out.scene.state == nullptr);
  }

  SECTION("the input's other fields survive; version and dirty behave")
  {
    auto in = make_state({camera_node(1, 0.f)});
    auto mat = std::make_shared<ossia::material_component>();
    in->materials = std::make_shared<std::vector<ossia::material_component_ptr>>(
        std::vector<ossia::material_component_ptr>{mat});
    n.inputs.scene_in.scene.state = in;

    n();
    const auto out = n.outputs.scene_out.scene.state;
    REQUIRE(out);
    CHECK(out.get() != in.get());
    CHECK(out->materials.get() == in->materials.get());
    CHECK(out->roots.get() == in->roots.get());
    CHECK(out->version == 1);
    CHECK(n.outputs.scene_out.dirty == 0xFF);

    n();
    CHECK(n.outputs.scene_out.scene.state == out);
    CHECK(n.outputs.scene_out.dirty == 0);

    in->version = 2;
    n();
    CHECK(n.outputs.scene_out.scene.state != out);
    CHECK(n.outputs.scene_out.scene.state->version == 2);
    CHECK(n.outputs.scene_out.dirty == 0xFF);
  }
}
