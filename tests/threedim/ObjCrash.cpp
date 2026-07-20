// L3 regression guard — split/threedim finding #1 (OBJ loader OOB read /
// crash). MUST-HAVE.
//
// TinyObj.cpp::ObjFromString copied vertices/texcoords/normals out of
// tinyobjloader's attrib arrays using file-controlled indices with NO range
// check. tinyobjloader does not bounds-check: a positive index far past the
// vertex count (fixIndex returns idx-1, unbounded) reads gigabytes past the
// buffer, and a face that omits a component leaves that component's index at
// its -1 default, indexing attrib[2*size_t(-1)]. Both are reachable straight
// from an attacker-controlled .obj and segfault the pre-fix loader.
//
// ObjFromString is a free function taking a std::string_view, so we drive the
// exact parse path directly with malformed inputs and assert it returns
// gracefully (empty or clamped) WITHOUT crashing. On the pre-fix engine these
// inputs segfault the test process — an unambiguous RED.

#include <Threedim/TinyObj.hpp>

#include <catch2/catch_test_macros.hpp>

using Threedim::ObjFromString;
using Threedim::float_vec;
using Threedim::mesh;

TEST_CASE(
    "OBJ face index far past the vertex count does not crash",
    "[threedim][obj][f1]")
{
  // 3 positions; the single face references vertex 100000000 (fixIndex ->
  // 99999999). Pre-fix: attrib.vertices[3*99999999] -> segfault.
  static constexpr std::string_view obj = R"(v 0 0 0
v 1 0 0
v 0 1 0
f 100000000 1 2
)";
  float_vec buf;
  std::vector<mesh> res = ObjFromString(obj, buf);
  // Graceful: the out-of-range face is rejected -> empty result.
  CHECK(res.empty());
}

TEST_CASE(
    "OBJ position-only faces with a texcoord attribute do not crash",
    "[threedim][obj][f1]")
{
  // `vt` makes attrib.texcoords non-empty (texcoord branch taken), but the
  // face is position-only so texcoord_index stays -1. Pre-fix:
  // attrib.texcoords[2*size_t(-1)] -> OOB read / crash.
  static constexpr std::string_view obj = R"(v 0 0 0
v 1 0 0
v 0 1 0
vt 0 0
f 1 2 3
)";
  float_vec buf;
  std::vector<mesh> res = ObjFromString(obj, buf);
  // A valid triangle is produced; the missing texcoords are defaulted, not
  // read out of bounds.
  REQUIRE(res.size() == 1);
  CHECK(res[0].vertices == 3);
}

TEST_CASE(
    "OBJ position-only faces with a normal attribute do not crash",
    "[threedim][obj][f1]")
{
  // `vn` makes attrib.normals non-empty (normal branch taken); the face omits
  // normals so normal_index stays -1. Pre-fix: attrib.normals[3*size_t(-1)].
  static constexpr std::string_view obj = R"(v 0 0 0
v 1 0 0
v 0 1 0
vn 0 0 1
f 1 2 3
)";
  float_vec buf;
  std::vector<mesh> res = ObjFromString(obj, buf);
  REQUIRE(res.size() == 1);
  CHECK(res[0].vertices == 3);
}

TEST_CASE(
    "OBJ out-of-range texcoord/normal indices do not crash",
    "[threedim][obj][f1]")
{
  // One vt + one vn, but the face references texcoord/normal 999999 (OOB
  // positive). Pre-fix: attrib.texcoords / attrib.normals indexed far past end.
  static constexpr std::string_view obj = R"(v 0 0 0
v 1 0 0
v 0 1 0
vt 0 0
vn 0 0 1
f 1/999999/999999 2/1/1 3/1/1
)";
  float_vec buf;
  std::vector<mesh> res = ObjFromString(obj, buf);
  // Vertex indices are valid, so a triangle survives with defaulted
  // texcoords/normals for the out-of-range corner.
  REQUIRE(res.size() == 1);
  CHECK(res[0].vertices == 3);
}

TEST_CASE("OBJ well-formed input still parses", "[threedim][obj][f1]")
{
  // Regression backstop: the bounds checks must not reject valid geometry.
  static constexpr std::string_view obj = R"(v 0 0 0
v 1 0 0
v 0 1 0
f 1 2 3
)";
  float_vec buf;
  std::vector<mesh> res = ObjFromString(obj, buf);
  REQUIRE(res.size() == 1);
  CHECK(res[0].vertices == 3);
}
