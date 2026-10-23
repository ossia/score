// The Flattened Scene Filter selects draws inside a merged batch by their tag
// and material (shader board P05). The Scene Preprocessor merges every regular
// draw into one geometry and stamped it tag 0 / material 0, so a tag or
// material filter kept or dropped the whole batch. The batch now carries the
// per-draw keys (a CPU "draw_filter_keys" auxiliary: tag_hash, material_index
// per draw command); the filter keeps the matching commands and draws them from
// the CPU command list. Pure CPU.
#include <Gfx/Graph/FlattenedSceneFilterNode.hpp>

#include <ossia/dataflow/geometry_port.hpp>
#include <ossia/detail/hash.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <memory>
#include <vector>

namespace
{
// A merged batch of three draws: (tag A, material 1), (tag B, material 2),
// (tag A, material 2), with a GPU indirect buffer.
ossia::geometry_spec batch()
{
  ossia::geometry g;
  for(uint32_t i = 0; i < 3; i++)
    g.cpu_draw_commands.push_back(
        {.index_or_vertex_count = 3, .instance_count = 1, .first_index_or_vertex = 3 * i,
         .base_vertex = 0, .first_instance = i});
  const uint32_t a = (uint32_t)ossia::hash_string("A");
  const uint32_t b = (uint32_t)ossia::hash_string("B");
  const uint32_t keys[6]{a, 1, b, 2, a, 2};
  auto data = std::shared_ptr<void>(new char[sizeof(keys)], [](void* p) { delete[] (char*)p; });
  std::memcpy(data.get(), keys, sizeof(keys));
  g.buffers.push_back({.data = ossia::geometry::cpu_buffer{data, (int64_t)sizeof(keys)}});
  g.auxiliary.push_back(
      {.name = "draw_filter_keys", .buffer = 0, .byte_offset = 0,
       .byte_size = (int64_t)sizeof(keys)});
  static int fake_buffer;
  g.indirect_count.handle = &fake_buffer;
  g.indirect_count.byte_size = 60;

  ossia::geometry_spec s;
  s.meshes = std::make_shared<ossia::mesh_list>();
  s.meshes->meshes.push_back(std::move(g));
  return s;
}

std::vector<uint32_t> firstInstances(const ossia::geometry_spec& s)
{
  std::vector<uint32_t> out;
  if(s.meshes)
    for(const auto& m : s.meshes->meshes)
      for(const auto& c : m.cpu_draw_commands)
        out.push_back(c.first_instance);
  return out;
}
}

TEST_CASE("the flattened filter selects draws in a merged batch", "[gfx][scene][P05]")
{
  using score::gfx::filterFlattenedScene;
  const auto in = batch();
  // Tag modes, by string (12 / 13) and by hash value (0 / 1).
  CHECK(firstInstances(filterFlattenedScene(in, 12, 0, "A")) == std::vector<uint32_t>{0, 2});
  CHECK(firstInstances(filterFlattenedScene(in, 13, 0, "A")) == std::vector<uint32_t>{1});
  CHECK(
      firstInstances(filterFlattenedScene(in, 0, (int)ossia::hash_string("B"), ""))
      == std::vector<uint32_t>{1});
  // Material modes.
  CHECK(firstInstances(filterFlattenedScene(in, 2, 2, "")) == std::vector<uint32_t>{1, 2});
  CHECK(firstInstances(filterFlattenedScene(in, 3, 2, "")) == std::vector<uint32_t>{0});
  // A filtered batch draws from its CPU commands: the GPU indirect buffer,
  // which holds every draw, is not kept.
  const auto f = filterFlattenedScene(in, 2, 1, "");
  REQUIRE(f.meshes);
  REQUIRE(f.meshes->meshes.size() == 1);
  CHECK(f.meshes->meshes[0].indirect_count.handle == nullptr);
  // Nothing matches: the batch is dropped.
  CHECK(filterFlattenedScene(in, 2, 7, "").meshes->meshes.empty());
}

TEST_CASE("the flattened filter keeps whole meshes without per-draw keys", "[gfx][scene][P05]")
{
  using score::gfx::filterFlattenedScene;
  ossia::geometry_spec s;
  s.meshes = std::make_shared<ossia::mesh_list>();
  ossia::geometry g;
  g.filter_tag = 5;
  s.meshes->meshes.push_back(g);
  CHECK(filterFlattenedScene(s, 0, 5, "").meshes->meshes.size() == 1);
  CHECK(filterFlattenedScene(s, 1, 5, "").meshes->meshes.empty());
}

namespace
{
// Four draws: (A, m1) and (B, m2) opaque, then (A, m2) and (B, m1) alpha-blended,
// the blended run published as _blend_draw_cmds (a range of the command list,
// 20-byte records), as the Scene Preprocessor does.
ossia::geometry_spec blendBatch()
{
  ossia::geometry g;
  for(uint32_t i = 0; i < 4; i++)
    g.cpu_draw_commands.push_back(
        {.index_or_vertex_count = 3, .instance_count = 1, .first_index_or_vertex = 3 * i,
         .base_vertex = 0, .first_instance = i});
  const uint32_t a = (uint32_t)ossia::hash_string("A");
  const uint32_t b = (uint32_t)ossia::hash_string("B");
  const uint32_t keys[8]{a, 1, b, 2, a, 2, b, 1};
  auto data = std::shared_ptr<void>(new char[sizeof(keys)], [](void* p) { delete[] (char*)p; });
  std::memcpy(data.get(), keys, sizeof(keys));
  g.buffers.push_back({.data = ossia::geometry::cpu_buffer{data, (int64_t)sizeof(keys)}});
  g.auxiliary.push_back(
      {.name = "draw_filter_keys", .buffer = 0, .byte_offset = 0,
       .byte_size = (int64_t)sizeof(keys)});
  static int fake_commands;
  g.buffers.push_back({.data = ossia::geometry::gpu_buffer{&fake_commands, 80}});
  g.auxiliary.push_back(
      {.name = "_blend_draw_cmds", .buffer = 1, .byte_offset = 2 * 20, .byte_size = 2 * 20});
  g.indirect_count.handle = &fake_commands;
  g.indirect_count.byte_size = 80;

  ossia::geometry_spec s;
  s.meshes = std::make_shared<ossia::mesh_list>();
  s.meshes->meshes.push_back(std::move(g));
  return s;
}

// The [first, count) of _blend_draw_cmds, in commands; {-1, -1} without it.
std::pair<int, int> blendRange(const ossia::geometry_spec& s)
{
  if(!s.meshes || s.meshes->meshes.size() != 1)
    return {-1, -1};
  if(const auto* aux = s.meshes->meshes[0].find_auxiliary("_blend_draw_cmds"))
    return {int(aux->byte_offset / 20), int(aux->byte_size / 20)};
  return {-1, -1};
}
}

// The blended run is a range of the command list: a filtered batch keeps
// fewer commands, so the range is that of the kept blended commands. It was
// left as the unfiltered list's, so the blend pass drew other draws, or none.
TEST_CASE("the flattened filter keeps the blended range of the commands it keeps", "[gfx][scene][P05]")
{
  using score::gfx::filterFlattenedScene;
  const auto in = blendBatch();
  // Tag A keeps (A, m1) opaque and (A, m2) blended: the blended run is command 1.
  const auto a = filterFlattenedScene(in, 12, 0, "A");
  CHECK(firstInstances(a) == std::vector<uint32_t>{0, 2});
  CHECK(blendRange(a) == std::pair{1, 1});
  // Material 1 keeps (A, m1) opaque and (B, m1) blended.
  CHECK(blendRange(filterFlattenedScene(in, 2, 1, "")) == std::pair{1, 1});
  // Material 2 keeps (B, m2) opaque and (A, m2) blended.
  const auto m2 = filterFlattenedScene(in, 2, 2, "");
  CHECK(firstInstances(m2) == std::vector<uint32_t>{1, 2});
  CHECK(blendRange(m2) == std::pair{1, 1});
  // Tag B then material 2 keeps (B, m2) opaque only: no blended run.
  const auto none = filterFlattenedScene(filterFlattenedScene(in, 12, 0, "B"), 2, 2, "");
  CHECK(firstInstances(none) == std::vector<uint32_t>{1});
  CHECK(blendRange(none) == std::pair{-1, -1});
}

// A filtered batch's keys are those of the commands it keeps, so a second
// filter selects among them. They were left as the unfiltered list's: the
// second filter found more keys than commands, fell back to the whole-batch
// tag / material (0), and dropped everything.
TEST_CASE("flattened filters compose", "[gfx][scene][P05]")
{
  using score::gfx::filterFlattenedScene;
  const auto in = blendBatch();
  const auto aThenM2 = filterFlattenedScene(filterFlattenedScene(in, 12, 0, "A"), 2, 2, "");
  CHECK(firstInstances(aThenM2) == std::vector<uint32_t>{2});
  CHECK(blendRange(aThenM2) == std::pair{0, 1});
  const auto bThenNotM2
      = filterFlattenedScene(filterFlattenedScene(in, 13, 0, "A"), 3, 2, "");
  CHECK(firstInstances(bThenNotM2) == std::vector<uint32_t>{3});
}
