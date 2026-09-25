// FBX skinning with a non-bone node between two bones.
//
// l1-fbx-rig-null-between-bones.fbx: LimbNode "Root" at the origin, a Null
// "Offset" child (T(0,2,0) Rz(90)), a LimbNode "Tip" under it (T(1,0,0)), so
// Tip's bind world is T(0,3,0) Rz(90). A triangle at (0,3,0) (1,3,0) (0,4,0)
// is fully weighted to Tip. Tip's joint parent is Root: its node-local rest
// alone put it at (1,0,0), so the bind pose skinned the triangle away from
// where it was modelled. In the bind pose every skinned vertex must stay put.
#include <Gfx/Graph/SceneGPUState.hpp>
#include <Threedim/FbxParser.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QMatrix4x4>

#include <string>

namespace
{
int jointNamed(const ossia::skeleton_component& sk, const std::string& name)
{
  for(std::size_t i = 0; i < sk.joints.size(); i++)
    if(sk.joints[i].name == name)
      return int(i);
  return -1;
}
}

TEST_CASE("FBX joint rest includes non-bone nodes between bones", "[threedim][fbx][skin][l1]")
{
  using fbx_port = Threedim::FbxParser::ins::fbx_t;
  fbx_port::file_type file{};
  file.filename = THREEDIM_TEST_DIR "/l1-fbx-rig-null-between-bones.fbx";
  auto apply = fbx_port::process(file);
  REQUIRE(apply);
  Threedim::FbxParser parser;
  apply(parser);
  REQUIRE(parser.m_raw_state);
  REQUIRE(parser.m_skeleton);

  const auto& sk = *parser.m_skeleton;
  const int root = jointNamed(sk, "Root");
  const int tip = jointNamed(sk, "Tip");
  REQUIRE(root >= 0);
  REQUIRE(tip >= 0);
  CHECK(sk.joints[root].parent_index == -1);
  const int tipParent = sk.joints[tip].parent_index;
  REQUIRE(tipParent >= 0);
  CHECK(sk.joints[tipParent].name == "Offset");
  CHECK(sk.joints[tipParent].parent_index == root);

  ossia::scene_spec spec;
  spec.state = parser.m_raw_state;
  score::gfx::FlatScene flat;
  score::gfx::flattenScene(spec, flat, 1.f);
  REQUIRE(flat.skins.size() == 1);
  const auto& jm = flat.skins[0].joint_matrices;
  REQUIRE(jm.size() == sk.joints.size());

  const Threedim::FbxParser::ScenePart* part = nullptr;
  for(const auto& n : parser.m_scene_nodes)
    for(const auto& p : n.parts)
      if(p.joints0 && p.weights0)
        part = &p;
  REQUIRE(part);
  REQUIRE(part->vertex_count == 3);

  for(uint32_t v = 0; v < part->vertex_count; v++)
  {
    const QVector3D pos(
        (*part->positions)[v * 3], (*part->positions)[v * 3 + 1],
        (*part->positions)[v * 3 + 2]);
    QMatrix4x4 skin;
    skin.fill(0.f);
    for(int k = 0; k < 4; k++)
    {
      const float w = (*part->weights0)[v * 4 + k];
      const uint16_t j = (*part->joints0)[v * 4 + k];
      if(w == 0.f)
        continue;
      REQUIRE(j < jm.size());
      skin += w * jm[j];
    }
    const QVector3D skinned = skin.map(pos);
    INFO("vertex " << v << " rest " << pos.x() << "," << pos.y() << "," << pos.z()
                   << " skinned " << skinned.x() << "," << skinned.y() << ","
                   << skinned.z());
    CHECK(pos.y() >= 3.f - 1e-4f);
    CHECK(skinned.x() == Catch::Approx(pos.x()).margin(1e-4));
    CHECK(skinned.y() == Catch::Approx(pos.y()).margin(1e-4));
    CHECK(skinned.z() == Catch::Approx(pos.z()).margin(1e-4));
  }
}
