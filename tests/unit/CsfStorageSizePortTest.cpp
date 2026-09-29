// A CSF storage buffer's element count is read from the long inlet that
// ISFNode synthesizes for a written buffer ending in a flexible array. A
// read_only buffer has no such inlet: its one inlet is the buffer itself, and
// reading it as the count found no value. CPU only: no RHI.
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/ISFVisitors.hpp>

#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
// The shape of the trail generator in the AI tracking example: a geometry
// resource first, then a read_only buffer fed by an upstream node.
constexpr auto readOnlyAfterGeometry = R"_(/*{
"ISFVSN": "2.0",
"MODE": "COMPUTE_SHADER",
"RESOURCES": [
  { "NAME": "geo", "TYPE": "geometry", "VERTEX_COUNT": 6, "INSTANCE_COUNT": "$maxParticles",
    "ATTRIBUTES": [
      { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_write", "RATE": "instance" }
    ] },
  { "NAME": "xyBuffer", "TYPE": "storage", "ACCESS": "read_only",
    "LAYOUT": [ { "NAME": "particles", "TYPE": "vec2[]" } ] },
  { "NAME": "maxParticles", "TYPE": "long", "DEFAULT": 100, "MIN": 1, "MAX": 2048 }
],
"PASSES": [ { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_INSTANCE" } } ]
}*/
void main() { }
)_";

constexpr auto writtenBuffers = R"_(/*{
"ISFVSN": "2.0",
"MODE": "COMPUTE_SHADER",
"RESOURCES": [
  { "NAME": "gain", "TYPE": "float", "DEFAULT": 1.0 },
  { "NAME": "input", "TYPE": "storage", "ACCESS": "read_only",
    "LAYOUT": [ { "NAME": "values", "TYPE": "float[]" } ] },
  { "NAME": "fixed", "TYPE": "storage", "ACCESS": "write_only",
    "LAYOUT": [ { "NAME": "sum", "TYPE": "float" } ] },
  { "NAME": "flexible", "TYPE": "storage", "ACCESS": "read_write",
    "LAYOUT": [ { "NAME": "count", "TYPE": "uint" }, { "NAME": "values", "TYPE": "vec4[]" } ] },
  { "NAME": "offset", "TYPE": "float", "DEFAULT": 0.0 }
],
"PASSES": [ { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "1D_BUFFER" } } ]
}*/
void main() { }
)_";

isf::descriptor parse(const char* src)
{
  isf::parser p{src, isf::parser::ShaderType::CSF};
  REQUIRE(p.mode() == isf::descriptor::CSF);
  return p.data();
}
}

TEST_CASE(
    "A read_only CSF storage buffer has no size inlet",
    "[gfx][csf][storage][ports]")
{
  const auto desc = parse(readOnlyAfterGeometry);
  CHECK(score::gfx::storage_array_size_port(desc, "xyBuffer") == -1);

  score::gfx::ISFNode node{desc, QString{}};
  REQUIRE(node.input.size() == 3);
  CHECK(node.input[0]->type == score::gfx::Types::Geometry);
  CHECK(node.input[1]->type == score::gfx::Types::Buffer);
  CHECK(node.input[2]->type == score::gfx::Types::Int);
}

TEST_CASE(
    "A written CSF storage buffer's flexible array is sized by its own inlet",
    "[gfx][csf][storage][ports]")
{
  const auto desc = parse(writtenBuffers);
  CHECK(score::gfx::storage_array_size_port(desc, "input") == -1);
  CHECK(score::gfx::storage_array_size_port(desc, "fixed") == -1);
  CHECK(score::gfx::storage_array_size_port(desc, "missing") == -1);

  const int port = score::gfx::storage_array_size_port(desc, "flexible");
  // gain, input, then the synthesized count of "flexible", then offset
  CHECK(port == 2);

  score::gfx::ISFNode node{desc, QString{}};
  REQUIRE(node.input.size() == 4);
  REQUIRE(port >= 0);
  REQUIRE(port < std::ssize(node.input));
  CHECK(node.input[port]->type == score::gfx::Types::Int);
  REQUIRE(node.input[port]->value != nullptr);
  CHECK(*static_cast<const int*>(node.input[port]->value) == 1024);
  CHECK(node.input[3]->type == score::gfx::Types::Float);
}
