// Unit coverage for Threedim/PCLToGeometry.cpp (PCLToMesh2, "Pointcloud to
// mesh"). The node is a descriptor builder: it does NOT read point data on the
// CPU. It takes an opaque GPU buffer (handle / byte_size / byte_offset) plus a
// BufferType layout enum and publishes a halp::dynamic_gpu_geometry that tells
// the renderer how to draw that buffer as a point cloud: one vertex buffer,
// one per-vertex binding whose stride matches the layout, position/color0
// attributes at the right byte offsets, points topology, and the vertex count
// derived from the buffer size.
//
// So the honest assertions are on the *descriptor*, computed on paper from the
// four layouts (XYZ=12B, XYZ_RGB=24B, XYZW=16B, XYZW_RGBA=32B per point): the
// handle passes through untouched, strides / attribute offsets / formats /
// semantics are exact, vertices == byte_size / stride, and the vectors are
// cleared on every call so re-running or switching layouts never accumulates
// stale bindings. The "GPU handle" is just an opaque pointer, so everything
// runs headless: no QRhi, no display, no files.
//
// Empty (0-byte, null-handle) and ragged (size not a multiple of the stride)
// buffers must come out safe: zero / floored vertex counts, no crash.

#include <Threedim/PCLToGeometry.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>

using Catch::Approx;

namespace
{
// An opaque "GPU" handle: the node must pass it through without dereferencing.
static float fake_gpu_storage[64]{};
static void* const fake_handle = fake_gpu_storage;

Threedim::PCLToMesh2 makeNode(
    Threedim::PCLToMesh2::BufferType type, int64_t byte_size,
    int64_t byte_offset = 0, void* handle = fake_handle)
{
  Threedim::PCLToMesh2 node;
  node.inputs.in.buffer.handle = handle;
  node.inputs.in.buffer.byte_size = byte_size;
  node.inputs.in.buffer.byte_offset = byte_offset;
  node.inputs.type.value = type;
  return node;
}

// The invariants every successful layout shares: one pass-through buffer, one
// per-vertex binding, one geometry input on buffer 0, points topology, no
// culling, and the dirty flag raised.
void checkCommon(Threedim::PCLToMesh2& node, int expected_stride)
{
  auto& g = node.outputs.geometry;
  auto& mesh = g.mesh;

  CHECK(g.dirty_mesh);
  CHECK(mesh.topology == halp::primitive_topology::points);
  CHECK(mesh.cull_mode == halp::cull_mode::none);
  CHECK(mesh.front_face == halp::front_face::counter_clockwise);

  REQUIRE(mesh.buffers.size() == 1);
  CHECK(mesh.buffers[0].handle == node.inputs.in.buffer.handle);
  CHECK(mesh.buffers[0].byte_size == node.inputs.in.buffer.byte_size);
  CHECK(mesh.buffers[0].dirty);

  REQUIRE(mesh.bindings.size() == 1);
  CHECK(mesh.bindings[0].stride == expected_stride);
  CHECK(mesh.bindings[0].step_rate == 1);
  CHECK(
      mesh.bindings[0].classification == halp::binding_classification::per_vertex);

  REQUIRE(mesh.input.size() == 1);
  CHECK(mesh.input[0].buffer == 0);
  CHECK(mesh.input[0].byte_offset == node.inputs.in.buffer.byte_offset);

  CHECK(mesh.indices == 0);
}
}

TEST_CASE(
    "PCLToMesh2 constructor publishes an identity transform and a dirty mesh",
    "[threedim][pcl]")
{
  Threedim::PCLToMesh2 node;

  CHECK(node.outputs.geometry.dirty_mesh);
  CHECK(node.outputs.geometry.dirty_transform);

  // Position (0,0,0), rotation (0,0,0), scale (1,1,1) -> identity, stored
  // column-major (QMatrix4x4::data() layout via TinyObj.hpp's toGL).
  const float* t = node.outputs.geometry.transform;
  for(int col = 0; col < 4; col++)
    for(int row = 0; row < 4; row++)
      CHECK(t[col * 4 + row] == Approx(col == row ? 1.f : 0.f).margin(1e-6));
}

TEST_CASE("PCLToMesh2 XYZ: float3 position, stride 12", "[threedim][pcl]")
{
  // 4 points x 3 floats = 48 bytes.
  auto node = makeNode(Threedim::PCLToMesh2::XYZ, 4 * 3 * sizeof(float));
  node();

  checkCommon(node, 12);
  auto& mesh = node.outputs.geometry.mesh;

  REQUIRE(mesh.attributes.size() == 1);
  CHECK(mesh.attributes[0].binding == 0);
  CHECK(mesh.attributes[0].semantic == halp::attribute_semantic::position);
  CHECK(mesh.attributes[0].format == halp::attribute_format::float3);
  CHECK(mesh.attributes[0].byte_offset == 0);

  CHECK(mesh.vertices == 4);
}

TEST_CASE(
    "PCLToMesh2 XYZ_RGB: interleaved float3 position + float3 color, stride 24",
    "[threedim][pcl]")
{
  // 3 points x 6 floats = 72 bytes.
  auto node = makeNode(Threedim::PCLToMesh2::XYZ_RGB, 3 * 6 * sizeof(float));
  node();

  checkCommon(node, 24);
  auto& mesh = node.outputs.geometry.mesh;

  REQUIRE(mesh.attributes.size() == 2);
  CHECK(mesh.attributes[0].semantic == halp::attribute_semantic::position);
  CHECK(mesh.attributes[0].format == halp::attribute_format::float3);
  CHECK(mesh.attributes[0].byte_offset == 0);
  CHECK(mesh.attributes[0].binding == 0);

  CHECK(mesh.attributes[1].semantic == halp::attribute_semantic::color0);
  CHECK(mesh.attributes[1].format == halp::attribute_format::float3);
  CHECK(mesh.attributes[1].byte_offset == 12); // colours start after xyz
  CHECK(mesh.attributes[1].binding == 0);

  CHECK(mesh.vertices == 3);
}

TEST_CASE("PCLToMesh2 XYZW: float4 position, stride 16", "[threedim][pcl]")
{
  // 5 points x 4 floats = 80 bytes.
  auto node = makeNode(Threedim::PCLToMesh2::XYZW, 5 * 4 * sizeof(float));
  node();

  checkCommon(node, 16);
  auto& mesh = node.outputs.geometry.mesh;

  REQUIRE(mesh.attributes.size() == 1);
  CHECK(mesh.attributes[0].semantic == halp::attribute_semantic::position);
  CHECK(mesh.attributes[0].format == halp::attribute_format::float4);
  CHECK(mesh.attributes[0].byte_offset == 0);

  CHECK(mesh.vertices == 5);
}

TEST_CASE(
    "PCLToMesh2 XYZW_RGBA: float4 position + float4 color, stride 32",
    "[threedim][pcl]")
{
  // 2 points x 8 floats = 64 bytes.
  auto node = makeNode(Threedim::PCLToMesh2::XYZW_RGBA, 2 * 8 * sizeof(float));
  node();

  checkCommon(node, 32);
  auto& mesh = node.outputs.geometry.mesh;

  REQUIRE(mesh.attributes.size() == 2);
  CHECK(mesh.attributes[0].semantic == halp::attribute_semantic::position);
  CHECK(mesh.attributes[0].format == halp::attribute_format::float4);
  CHECK(mesh.attributes[0].byte_offset == 0);

  CHECK(mesh.attributes[1].semantic == halp::attribute_semantic::color0);
  CHECK(mesh.attributes[1].format == halp::attribute_format::float4);
  CHECK(mesh.attributes[1].byte_offset == 16); // colours start after xyzw

  CHECK(mesh.vertices == 2);
}

TEST_CASE(
    "PCLToMesh2 empty cloud: zero bytes, null handle -> zero vertices, no crash",
    "[threedim][pcl]")
{
  auto node = makeNode(Threedim::PCLToMesh2::XYZ, 0, 0, nullptr);
  node();

  checkCommon(node, 12);
  auto& mesh = node.outputs.geometry.mesh;
  CHECK(mesh.vertices == 0);
  CHECK(mesh.buffers[0].handle == nullptr);
  CHECK(mesh.buffers[0].byte_size == 0);
}

TEST_CASE(
    "PCLToMesh2 ragged cloud: size not a stride multiple floors the count",
    "[threedim][pcl]")
{
  // 50 bytes of XYZ data = 4 whole points + 2 trailing bytes. The renderer
  // must never be told to draw the partial 5th point.
  auto node = makeNode(Threedim::PCLToMesh2::XYZ, 50);
  node();

  checkCommon(node, 12);
  CHECK(node.outputs.geometry.mesh.vertices == 4);
}

TEST_CASE(
    "PCLToMesh2 re-run and layout switch never accumulate stale descriptors",
    "[threedim][pcl]")
{
  // First tick as the widest layout (2 attributes)...
  auto node = makeNode(Threedim::PCLToMesh2::XYZW_RGBA, 2 * 8 * sizeof(float));
  node();
  REQUIRE(node.outputs.geometry.mesh.attributes.size() == 2);

  // ...then the same tick again: everything must be rebuilt, not appended.
  node();
  checkCommon(node, 32);
  CHECK(node.outputs.geometry.mesh.attributes.size() == 2);
  CHECK(node.outputs.geometry.mesh.vertices == 2);

  // ...then switch to XYZ: the color0 attribute must be gone and the stride,
  // count and dirty flag must reflect the new layout.
  node.inputs.type.value = Threedim::PCLToMesh2::XYZ;
  node.inputs.in.buffer.byte_size = 4 * 3 * sizeof(float);
  node.outputs.geometry.dirty_mesh = false;
  node();

  checkCommon(node, 12);
  auto& mesh = node.outputs.geometry.mesh;
  REQUIRE(mesh.attributes.size() == 1);
  CHECK(mesh.attributes[0].semantic == halp::attribute_semantic::position);
  CHECK(mesh.attributes[0].format == halp::attribute_format::float3);
  CHECK(mesh.vertices == 4);
  CHECK(node.outputs.geometry.dirty_mesh);
}

// DEFECT: with a non-zero byte_offset the vertex count is still computed from
// the FULL buffer size (byte_size / stride) in PCLToGeometry.cpp:
//
//   outputs.geometry.mesh.vertices = (tex.byte_size / (sizeof(float) * vertice_stride));
//
// while the geometry input starts the fetch at byte_offset inside that same
// buffer. halp's buffer convention (halp::raw_buffer in halp/buffer.hpp) is
// that byte_offset addresses INTO byte_size — usable bytes are
// byte_size - byte_offset — so drawing byte_size/stride vertices from
// byte_offset over-reads the buffer tail by byte_offset bytes on the GPU.
// Correct expectation: vertices == (byte_size - byte_offset) / stride.
// Tagged [!shouldfail]; flips red-to-green the day the count is fixed.
TEST_CASE(
    "PCLToMesh2 vertex count must exclude the byte_offset region",
    "[threedim][pcl][!shouldfail]")
{
  // 5 XYZ points' worth of bytes, but the input starts one point in:
  // only 4 points are addressable after the offset.
  auto node = makeNode(Threedim::PCLToMesh2::XYZ, 5 * 3 * sizeof(float), 12);
  node();

  auto& mesh = node.outputs.geometry.mesh;
  REQUIRE(mesh.input.size() == 1);
  CHECK(mesh.input[0].byte_offset == 12);
  CHECK(mesh.vertices == 4); // currently 5: reads 12 bytes past the end
}
