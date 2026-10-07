// A glTF skin keeps its joint hierarchy.
//
// skin-two-joints.gltf: node "Rig" at (2, 0, 0) holds joint "Root" at local
// (0, 1, 0), whose child joint "Child" sits at local (0, 1, 0); the skinned
// mesh node is at the scene root, so in its space the joints rest at (2, 1, 0)
// and (2, 2, 0), which the inverse bind matrices undo. The triangle around
// (2, 2, 0) is weighted fully to Child. The animation "bend" turns Root by 90
// degrees about Z at t = 1.
//
// In the rest pose every joint matrix is the identity: the mesh is drawn as
// authored. Animated, Child follows Root: Child's origin moves to
// (2, 1, 0) + Rz(90) (0, 1, 0) = (1, 1, 0).

#include <Gfx/Graph/SceneGPUState.hpp>
#include <Threedim/AnimationPlayer.hpp>
#include <Threedim/GltfParser.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QMatrix4x4>
#include <QVector3D>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

using Catch::Approx;

namespace
{
std::shared_ptr<const ossia::scene_state> load()
{
  using gltf_port = Threedim::GltfParser::ins::gltf_t;
  gltf_port::file_type file{};
  file.filename = THREEDIM_TEST_DIR "/skin-two-joints.gltf";
  auto apply = gltf_port::process(file);
  REQUIRE(apply);
  Threedim::GltfParser parser;
  apply(parser);
  REQUIRE(parser.m_raw_state);
  return parser.m_raw_state;
}

std::vector<QMatrix4x4> joint_matrices(std::shared_ptr<const ossia::scene_state> s)
{
  ossia::scene_spec spec;
  spec.state = std::move(s);
  score::gfx::FlatScene flat;
  score::gfx::flattenScene(spec, flat, 1.f);
  REQUIRE(flat.skins.size() == 1);
  return flat.skins[0].joint_matrices;
}
}

TEST_CASE("glTF skin joints keep their parents and rest pose", "[threedim][gltf][skin]")
{
  const auto scene = load();
  REQUIRE(scene->skeletons);
  REQUIRE(scene->skeletons->size() == 1);
  const auto& sk = *(*scene->skeletons)[0];
  REQUIRE(sk.joints.size() >= 2);
  CHECK(sk.joints[0].name == "Root");
  CHECK(sk.joints[1].name == "Child");
  CHECK(sk.joints[1].parent_index == 0);
  CHECK(sk.joints[1].translation[1] == Approx(1.f));

  SECTION("in the rest pose the skinned vertex is unchanged")
  {
    const auto jm = joint_matrices(scene);
    REQUIRE(jm.size() >= 2);
    for(int j = 0; j < 2; ++j)
    {
      const QVector3D v = jm[j].map(QVector3D(2.f, 2.f, 0.f));
      INFO("joint " << j << " maps (2, 2, 0) to " << v.x() << " " << v.y() << " "
                    << v.z());
      CHECK(v.x() == Approx(2.f).margin(1e-4));
      CHECK(v.y() == Approx(2.f).margin(1e-4));
      CHECK(v.z() == Approx(0.f).margin(1e-4));
    }
  }

  SECTION("animated, the child joint follows its parent")
  {
    Threedim::AnimationPlayer player;
    player.inputs.scene_in.scene.state = scene;
    player.inputs.time.value = 1.f;
    player.inputs.speed.value = 0.f;
    player.inputs.loop.value = false;
    player.inputs.clip_index.value = 0;
    player();
    REQUIRE(player.outputs.scene_out.scene.state);

    const auto jm = joint_matrices(player.outputs.scene_out.scene.state);
    REQUIRE(jm.size() >= 2);
    const QVector3D v = jm[1].map(QVector3D(2.f, 2.f, 0.f));
    INFO("child maps (2, 2, 0) to " << v.x() << " " << v.y() << " " << v.z());
    CHECK(v.x() == Approx(1.f).margin(1e-4));
    CHECK(v.y() == Approx(1.f).margin(1e-4));
    CHECK(v.z() == Approx(0.f).margin(1e-4));
  }
}
