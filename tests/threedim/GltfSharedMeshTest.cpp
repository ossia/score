// Nodes that instance the same glTF mesh share one mesh primitive identity.
//
// The scene preprocessor keeps one GPU copy of a mesh per mesh_primitive
// stable_id. The glTF loader extracted a mesh again for every node using it,
// each copy with a fresh id, so a model that places one mesh many times (a
// chess set: 4.5M indices instead of 1.72M) filled the shared index pool.
#include <Threedim/GltfParser.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace
{
void collect(const ossia::scene_node& n, std::vector<const ossia::mesh_primitive*>& out)
{
  if(!n.children)
    return;
  for(const auto& p : *n.children)
  {
    if(auto* mc = ossia::get_if<ossia::mesh_component_ptr>(&p); mc && *mc)
      for(const auto& prim : (*mc)->primitives)
        out.push_back(&prim);
    if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&p); sub && *sub)
      collect(**sub, out);
  }
}
}

TEST_CASE("glTF nodes instancing one mesh share its primitive id", "[threedim][gltf]")
{
  using gltf_port = Threedim::GltfParser::ins::gltf_t;
  gltf_port::file_type file{};
  const std::string path = THREEDIM_TEST_DIR "/shared-mesh-two-nodes.gltf";
  file.filename = path;

  auto apply = gltf_port::process(file);
  REQUIRE(apply);
  Threedim::GltfParser parser;
  apply(parser);
  const auto scene = parser.m_raw_state;
  REQUIRE(scene);
  REQUIRE(scene->roots);

  std::vector<const ossia::mesh_primitive*> prims;
  for(const auto& r : *scene->roots)
    if(r)
      collect(*r, prims);

  REQUIRE(prims.size() == 2);
  CHECK(prims[0]->stable_id != 0);
  CHECK(prims[0]->stable_id == prims[1]->stable_id);
  CHECK(prims[0]->vertex_count == 3);
  CHECK(prims[1]->vertex_count == 3);
}
