// Unit coverage for Threedim/BufferToGeometry.cpp (BuffersToGeometry,
// deprecated v1) and Threedim/BufferToGeometry2.cpp (BuffersToGeometry2) —
// the "Buffers to geometry" nodes. Like PCLToMesh2 they are descriptor
// builders: no point data is read on the CPU. They take up to 8 opaque GPU
// buffers (handle / byte_size / byte_offset) plus per-attribute layout
// controls (source buffer, offset, stride, format, instanced) and publish a
// halp::dynamic_gpu_geometry telling the renderer how to draw them.
//
// The honest assertions are on the *descriptor*, computed on paper:
// - buffer handle/byte_size pass through untouched and shared source buffers
//   are deduplicated into one descriptor entry;
// - one binding per enabled attribute (stride = control, or the format's
//   byte size when the control is 0), per_vertex/per_instance classification;
// - geometry_input.byte_offset == attribute offset + buffer view byte_offset
//   (the arithmetic PCLToMesh2 gets wrong is done RIGHT here — pinned green);
// - vertices/instances are explicit controls passed through verbatim (no
//   size-derived count exists to floor, unlike PCLToMesh2);
// - index buffer appended with summed byte offset and 16/32-bit format,
//   index.buffer == -1 when disabled or the index source is null/empty;
// - topology / cull mode / front face mapping;
// - re-runs and layout switches rebuild, never append, stale entries;
// - a null attribute-source handle wipes the descriptor without crashing and
//   the next valid tick rebuilds it (m_prevVertices = -1 reanalysis hack).
//
// What distinguishes the siblings (both covered):
// - v1 maps a per-attribute "location" spinbox to the semantic through a
//   fixed table (0..4 -> position/texcoord0/color0/normal/tangent, else
//   custom; when instanced, 0/3/4 remap to translation/rotation/scale);
// - v2 replaces the location control with a free-text semantic name resolved
//   case-insensitively (with aliases like "uv") via ossia::name_to_semantic,
//   unknown or empty names resolving to custom. Note v2 does not copy the
//   unrecognized name into geometry_attribute::name (whose contract says
//   custom semantics carry their string), and keeps a dead attr_idx counter.
//
// DEFECTS found and pinned [!shouldfail] (never blessed as green):
// 1) toHalpFormat/attributeFormatSize in BufferToGeometryCommon.hpp
//    static_cast the local AttributeFormat enum (QRhi-style order WITHOUT
//    UInt3/SInt3/UShort3/SShort3) into halp::attribute_format (which HAS
//    uint3/sint3/ushort3/sshort1..4), so every value from UInt2 (8) upward
//    publishes the wrong format and the wrong default stride.
// 2) The null-handle early return wipes the descriptor after mesh.*.clear()
//    but never raises dirty_mesh, so after a preceding no-change tick the
//    wipe ships with dirty_mesh == false and downstream keeps rendering a
//    GPU geometry referencing the old (possibly freed) buffer handle.
//
// Everything runs headless: handles are opaque pointers, no QRhi, no files.

#include <Threedim/BufferToGeometry.hpp>
#include <Threedim/BufferToGeometry2.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>

using Catch::Approx;

namespace
{
using V1 = Threedim::BuffersToGeometry;
using V2 = Threedim::BuffersToGeometry2;

// Opaque "GPU" handles: the nodes must pass them through without ever
// dereferencing them.
static float fake_storage_a[64]{};
static float fake_storage_b[64]{};
static float fake_storage_c[64]{};
void* const handle_a = fake_storage_a;
void* const handle_b = fake_storage_b;
void* const handle_c = fake_storage_c;

template <typename Node>
halp::gpu_buffer& buffer(Node& node, int i)
{
  switch(i)
  {
    case 0:
      return node.inputs.buffer_0.buffer;
    case 1:
      return node.inputs.buffer_1.buffer;
    case 2:
      return node.inputs.buffer_2.buffer;
    default:
      return node.inputs.buffer_3.buffer;
  }
}

template <typename Node>
void setBuffer(Node& node, int i, void* h, int64_t size, int64_t byte_offset = 0)
{
  auto& b = buffer(node, i);
  b.handle = h;
  b.byte_size = size;
  b.byte_offset = byte_offset;
  b.changed = false;
}

// The controls shared by both siblings (v1 additionally has location_N,
// v2 semantic_N — set by the per-node setAttr overloads below).
template <typename Node>
void setAttrCommon(
    Node& node, int slot, int buf, int offset, int stride,
    Threedim::AttributeFormat fmt, bool instanced)
{
  switch(slot)
  {
#define ATTR_CASE(n)                               \
  case n:                                          \
    node.inputs.attribute_buffer_##n.value = buf;  \
    node.inputs.attribute_offset_##n.value = offset; \
    node.inputs.attribute_stride_##n.value = stride; \
    node.inputs.format_##n.value = fmt;            \
    node.inputs.instanced_##n.value = instanced;   \
    break;
    ATTR_CASE(0)
    ATTR_CASE(1)
    ATTR_CASE(2)
    ATTR_CASE(3)
#undef ATTR_CASE
    default:
      break;
  }
}

void setAttr(
    V1& node, int slot, int buf, int offset, int stride,
    Threedim::AttributeFormat fmt, int location, bool instanced = false)
{
  setAttrCommon(node, slot, buf, offset, stride, fmt, instanced);
  switch(slot)
  {
    case 0:
      node.inputs.location_0.value = location;
      break;
    case 1:
      node.inputs.location_1.value = location;
      break;
    case 2:
      node.inputs.location_2.value = location;
      break;
    default:
      node.inputs.location_3.value = location;
      break;
  }
}

void setAttr(
    V2& node, int slot, int buf, int offset, int stride,
    Threedim::AttributeFormat fmt, std::string semantic, bool instanced = false)
{
  setAttrCommon(node, slot, buf, offset, stride, fmt, instanced);
  switch(slot)
  {
    case 0:
      node.inputs.semantic_0.value = std::move(semantic);
      break;
    case 1:
      node.inputs.semantic_1.value = std::move(semantic);
      break;
    case 2:
      node.inputs.semantic_2.value = std::move(semantic);
      break;
    default:
      node.inputs.semantic_3.value = std::move(semantic);
      break;
  }
}

} // namespace

// -----------------------------------------------------------------------------
// Behavior shared by both siblings. Semantics are asserted in the per-node
// sections (they are the one axis on which v1 and v2 differ).
// -----------------------------------------------------------------------------

TEMPLATE_TEST_CASE(
    "BuffersToGeometry constructor publishes an identity transform and clean flags",
    "[threedim][buffertogeometry]", V1, V2)
{
  TestType node;

  CHECK(!node.outputs.geometry.dirty_mesh);
  CHECK(!node.outputs.geometry.dirty_transform);

  const float* t = node.outputs.geometry.transform;
  for(int col = 0; col < 4; col++)
    for(int row = 0; row < 4; row++)
      CHECK(t[col * 4 + row] == Approx(col == row ? 1.f : 0.f).margin(1e-6));
}

TEMPLATE_TEST_CASE(
    "single float3 attribute: pass-through buffer, tightly-packed binding, "
    "explicit vertex/instance counts",
    "[threedim][buffertogeometry]", V1, V2)
{
  TestType node;
  // 4 vertices' worth of xyz in buffer 0.
  setBuffer(node, 0, handle_a, 4 * 3 * sizeof(float));
  setAttrCommon(node, 0, /*buf*/ 0, /*offset*/ 0, /*stride*/ 0,
                Threedim::AttributeFormat::Float3, /*instanced*/ false);
  node.inputs.vertices.value = 4;
  node.inputs.instances.value = 3;
  node();

  auto& g = node.outputs.geometry;
  auto& mesh = g.mesh;

  CHECK(g.dirty_mesh);

  REQUIRE(mesh.buffers.size() == 1);
  CHECK(mesh.buffers[0].handle == handle_a);
  CHECK(mesh.buffers[0].byte_size == 48);
  CHECK(mesh.buffers[0].dirty);

  REQUIRE(mesh.bindings.size() == 1);
  // Stride control is 0 -> tightly packed: float3 = 12 bytes.
  CHECK(mesh.bindings[0].stride == 12);
  CHECK(mesh.bindings[0].step_rate == 1);
  CHECK(mesh.bindings[0].classification == halp::binding_classification::per_vertex);

  REQUIRE(mesh.attributes.size() == 1);
  CHECK(mesh.attributes[0].binding == 0);
  CHECK(mesh.attributes[0].format == halp::attribute_format::float3);
  // The offset lives on the geometry_input, never on the attribute.
  CHECK(mesh.attributes[0].byte_offset == 0);

  REQUIRE(mesh.input.size() == 1);
  CHECK(mesh.input[0].buffer == 0);
  CHECK(mesh.input[0].byte_offset == 0);

  // Vertex/instance counts are explicit controls, passed through verbatim:
  // there is no byte_size-derived arithmetic here (contrast PCLToMesh2).
  CHECK(mesh.vertices == 4);
  CHECK(mesh.instances == 3);

  CHECK(mesh.topology == halp::primitive_topology::triangles);
  CHECK(mesh.cull_mode == halp::cull_mode::none);
  CHECK(mesh.front_face == halp::front_face::counter_clockwise);
  CHECK(mesh.index.buffer == -1);
}

TEMPLATE_TEST_CASE(
    "attribute offset and buffer view byte_offset sum into the geometry input",
    "[threedim][buffertogeometry]", V1, V2)
{
  // The buffer view starts 16 bytes into the underlying allocation and the
  // attribute another 8 bytes into the view: the fetch must start at 24.
  // This is the arithmetic PCLToMesh2 gets wrong; these nodes do it right,
  // so it is pinned green.
  TestType node;
  setBuffer(node, 0, handle_a, 128, /*byte_offset*/ 16);
  setAttrCommon(node, 0, 0, /*offset*/ 8, /*stride*/ 32,
                Threedim::AttributeFormat::Float4, false);
  node.inputs.vertices.value = 3;
  node();

  auto& mesh = node.outputs.geometry.mesh;
  REQUIRE(mesh.input.size() == 1);
  CHECK(mesh.input[0].byte_offset == 24);
  REQUIRE(mesh.bindings.size() == 1);
  CHECK(mesh.bindings[0].stride == 32);
  REQUIRE(mesh.attributes.size() == 1);
  CHECK(mesh.attributes[0].byte_offset == 0);
  // The full underlying size passes through; the view offset is not
  // subtracted from it (it is applied per-input instead).
  REQUIRE(mesh.buffers.size() == 1);
  CHECK(mesh.buffers[0].byte_size == 128);
}

TEMPLATE_TEST_CASE(
    "two interleaved attributes sharing one buffer deduplicate the buffer entry",
    "[threedim][buffertogeometry]", V1, V2)
{
  // Interleaved [xyz rgb] x 4, stride 24; colours 12 bytes in.
  TestType node;
  setBuffer(node, 0, handle_a, 4 * 24);
  setAttrCommon(node, 0, 0, /*offset*/ 0, /*stride*/ 24,
                Threedim::AttributeFormat::Float3, false);
  setAttrCommon(node, 1, 0, /*offset*/ 12, /*stride*/ 24,
                Threedim::AttributeFormat::Float3, false);
  node.inputs.vertices.value = 4;
  node();

  auto& mesh = node.outputs.geometry.mesh;
  REQUIRE(mesh.buffers.size() == 1); // shared source deduplicated
  CHECK(mesh.buffers[0].handle == handle_a);

  REQUIRE(mesh.bindings.size() == 2);
  CHECK(mesh.bindings[0].stride == 24);
  CHECK(mesh.bindings[1].stride == 24);

  REQUIRE(mesh.attributes.size() == 2);
  CHECK(mesh.attributes[0].binding == 0);
  CHECK(mesh.attributes[1].binding == 1); // one binding per attribute

  REQUIRE(mesh.input.size() == 2);
  CHECK(mesh.input[0].buffer == 0);
  CHECK(mesh.input[0].byte_offset == 0);
  CHECK(mesh.input[1].buffer == 0);
  CHECK(mesh.input[1].byte_offset == 12);
}

TEMPLATE_TEST_CASE(
    "attributes on distinct, non-contiguous source buffers map in slot order",
    "[threedim][buffertogeometry]", V1, V2)
{
  TestType node;
  setBuffer(node, 0, handle_a, 48);
  setBuffer(node, 2, handle_c, 64);
  setAttrCommon(node, 0, /*buf*/ 0, 0, 0, Threedim::AttributeFormat::Float3, false);
  setAttrCommon(node, 1, /*buf*/ 2, 0, 0, Threedim::AttributeFormat::Float4, false);
  node.inputs.vertices.value = 4;
  node();

  auto& mesh = node.outputs.geometry.mesh;
  REQUIRE(mesh.buffers.size() == 2);
  CHECK(mesh.buffers[0].handle == handle_a);
  CHECK(mesh.buffers[1].handle == handle_c);
  REQUIRE(mesh.input.size() == 2);
  CHECK(mesh.input[0].buffer == 0);
  CHECK(mesh.input[1].buffer == 1);
}

TEMPLATE_TEST_CASE(
    "instanced attribute gets a per_instance binding",
    "[threedim][buffertogeometry]", V1, V2)
{
  TestType node;
  setBuffer(node, 0, handle_a, 48);
  setBuffer(node, 1, handle_b, 64);
  setAttrCommon(node, 0, 0, 0, 0, Threedim::AttributeFormat::Float3, false);
  setAttrCommon(node, 1, 1, 0, 0, Threedim::AttributeFormat::Float4,
                /*instanced*/ true);
  node.inputs.vertices.value = 4;
  node.inputs.instances.value = 4;
  node();

  auto& mesh = node.outputs.geometry.mesh;
  REQUIRE(mesh.bindings.size() == 2);
  CHECK(mesh.bindings[0].classification == halp::binding_classification::per_vertex);
  CHECK(
      mesh.bindings[1].classification == halp::binding_classification::per_instance);
  CHECK(mesh.bindings[1].step_rate == 1);
}

TEMPLATE_TEST_CASE(
    "topology, cull mode and front face controls map onto the halp enums",
    "[threedim][buffertogeometry]", V1, V2)
{
  TestType node;
  setBuffer(node, 0, handle_a, 48);
  setAttrCommon(node, 0, 0, 0, 0, Threedim::AttributeFormat::Float3, false);
  node.inputs.vertices.value = 4;
  node.inputs.topology.value = Threedim::PrimitiveTopology::LineStrip;
  node.inputs.cull_mode.value = Threedim::CullMode::Back;
  node.inputs.front_face.value = Threedim::FrontFace::Clockwise;
  node();

  auto& mesh = node.outputs.geometry.mesh;
  CHECK(mesh.topology == halp::primitive_topology::line_strip);
  CHECK(mesh.cull_mode == halp::cull_mode::back);
  CHECK(mesh.front_face == halp::front_face::clockwise);

  // Changing only the topology must trigger a rebuild.
  node.inputs.topology.value = Threedim::PrimitiveTopology::Points;
  node();
  CHECK(node.outputs.geometry.dirty_mesh);
  CHECK(mesh.topology == halp::primitive_topology::points);
}

TEMPLATE_TEST_CASE(
    "index buffer: appended entry, summed byte offset, 16/32-bit flag, "
    "clean -1 when disabled or null",
    "[threedim][buffertogeometry]", V1, V2)
{
  TestType node;
  setBuffer(node, 0, handle_a, 48);
  setBuffer(node, 1, handle_b, 24, /*byte_offset*/ 2);
  setAttrCommon(node, 0, 0, 0, 0, Threedim::AttributeFormat::Float3, false);
  node.inputs.vertices.value = 4;
  node.inputs.index_buffer.value = 1;
  node.inputs.index_format.value = Threedim::IndexFormat::UInt16;
  node.inputs.index_offset.value = 4;
  node();

  auto& mesh = node.outputs.geometry.mesh;
  // Attribute buffer at 0, index buffer appended after it.
  REQUIRE(mesh.buffers.size() == 2);
  CHECK(mesh.buffers[1].handle == handle_b);
  CHECK(mesh.buffers[1].byte_size == 24);
  CHECK(mesh.index.buffer == 1);
  CHECK(mesh.index.byte_offset == 6); // control offset 4 + view offset 2
  CHECK(mesh.index.format == halp::index_format::uint16);

  // 32-bit flag: an index_format change alone must rebuild.
  node.inputs.index_format.value = Threedim::IndexFormat::UInt32;
  node();
  CHECK(node.outputs.geometry.dirty_mesh);
  CHECK(mesh.index.format == halp::index_format::uint32);

  // Disabled -> -1 and no appended entry.
  node.inputs.index_buffer.value = -1;
  node();
  CHECK(mesh.index.buffer == -1);
  CHECK(mesh.index.byte_offset == 0);
  CHECK(mesh.buffers.size() == 1);

  // Pointing at a slot whose handle is null -> -1, no crash.
  node.inputs.index_buffer.value = 3;
  node();
  CHECK(mesh.index.buffer == -1);
  CHECK(mesh.index.byte_offset == 0);
}

TEMPLATE_TEST_CASE(
    "no-change tick: dirty_mesh false, per-buffer dirty tracks the input's "
    "changed flag",
    "[threedim][buffertogeometry]", V1, V2)
{
  TestType node;
  setBuffer(node, 0, handle_a, 48);
  setAttrCommon(node, 0, 0, 0, 0, Threedim::AttributeFormat::Float3, false);
  node.inputs.vertices.value = 4;
  node();
  REQUIRE(node.outputs.geometry.mesh.buffers.size() == 1);

  // Identical tick: nothing rebuilt, buffer not marked dirty.
  node();
  CHECK(!node.outputs.geometry.dirty_mesh);
  CHECK(!node.outputs.geometry.mesh.buffers[0].dirty);

  // Same handle but new contents: the changed flag must flow through.
  node.inputs.buffer_0.buffer.changed = true;
  node();
  CHECK(!node.outputs.geometry.dirty_mesh);
  CHECK(node.outputs.geometry.mesh.buffers[0].dirty);
}

TEMPLATE_TEST_CASE(
    "re-run and layout switch never accumulate stale descriptors",
    "[threedim][buffertogeometry]", V1, V2)
{
  TestType node;
  setBuffer(node, 0, handle_a, 96);
  setAttrCommon(node, 0, 0, 0, 24, Threedim::AttributeFormat::Float3, false);
  setAttrCommon(node, 1, 0, 12, 24, Threedim::AttributeFormat::Float3, false);
  node.inputs.vertices.value = 4;
  node();

  auto& mesh = node.outputs.geometry.mesh;
  REQUIRE(mesh.attributes.size() == 2);

  // Forced rebuild (vertex count change): rebuilt, not appended.
  node.inputs.vertices.value = 6;
  node();
  CHECK(node.outputs.geometry.dirty_mesh);
  CHECK(mesh.buffers.size() == 1);
  CHECK(mesh.bindings.size() == 2);
  CHECK(mesh.attributes.size() == 2);
  CHECK(mesh.input.size() == 2);
  CHECK(mesh.vertices == 6);

  // Layout shrink: disabling an attribute drops its whole chain.
  node.inputs.attribute_buffer_1.value = -1;
  node();
  CHECK(mesh.buffers.size() == 1);
  CHECK(mesh.bindings.size() == 1);
  CHECK(mesh.attributes.size() == 1);
  CHECK(mesh.input.size() == 1);
}

TEMPLATE_TEST_CASE(
    "null attribute-source handle: descriptor wiped without crash, next valid "
    "tick rebuilds in full",
    "[threedim][buffertogeometry]", V1, V2)
{
  TestType node;
  setBuffer(node, 0, handle_a, 48);
  setAttrCommon(node, 0, 0, 0, 0, Threedim::AttributeFormat::Float3, false);
  node.inputs.vertices.value = 4;
  node();
  REQUIRE(node.outputs.geometry.mesh.buffers.size() == 1);

  // The source vanishes: no geometry may survive, and no crash.
  node.inputs.buffer_0.buffer.handle = nullptr;
  node();
  auto& mesh = node.outputs.geometry.mesh;
  CHECK(mesh.buffers.empty());
  CHECK(mesh.bindings.empty());
  CHECK(mesh.attributes.empty());
  CHECK(mesh.input.empty());

  // The m_prevVertices = -1 reanalysis hack must force a full rebuild once a
  // valid handle is back, even though no control changed in between.
  node.inputs.buffer_0.buffer.handle = handle_a;
  node();
  CHECK(node.outputs.geometry.dirty_mesh);
  REQUIRE(mesh.buffers.size() == 1);
  CHECK(mesh.buffers[0].handle == handle_a);
  CHECK(mesh.bindings.size() == 1);
  CHECK(mesh.attributes.size() == 1);
  CHECK(mesh.input.size() == 1);
}

TEMPLATE_TEST_CASE(
    "transform: first tick dirty, identity by default, translation lands in "
    "the fourth column",
    "[threedim][buffertogeometry]", V1, V2)
{
  TestType node;
  setBuffer(node, 0, handle_a, 48);
  setAttrCommon(node, 0, 0, 0, 0, Threedim::AttributeFormat::Float3, false);
  node.inputs.vertices.value = 4;

  // First tick warms the TRS cache: dirty, and identity for default knobs.
  node();
  CHECK(node.outputs.geometry.dirty_transform);
  const float* t = node.outputs.geometry.transform;
  for(int col = 0; col < 4; col++)
    for(int row = 0; row < 4; row++)
      CHECK(t[col * 4 + row] == Approx(col == row ? 1.f : 0.f).margin(1e-6));

  // Untouched knobs: no transform dirt.
  node();
  CHECK(!node.outputs.geometry.dirty_transform);

  // Moving position dirties and lands column-major at indices 12/13/14.
  node.inputs.position.value.x = 1.f;
  node.inputs.position.value.y = 2.f;
  node.inputs.position.value.z = 3.f;
  node();
  CHECK(node.outputs.geometry.dirty_transform);
  CHECK(t[12] == Approx(1.f).margin(1e-6));
  CHECK(t[13] == Approx(2.f).margin(1e-6));
  CHECK(t[14] == Approx(3.f).margin(1e-6));
  CHECK(t[15] == Approx(1.f).margin(1e-6));
  CHECK(t[0] == Approx(1.f).margin(1e-6));
  CHECK(t[5] == Approx(1.f).margin(1e-6));
  CHECK(t[10] == Approx(1.f).margin(1e-6));
}

TEMPLATE_TEST_CASE(
    "formats up to UInt4 map onto halp correctly (the aligned enum prefix)",
    "[threedim][buffertogeometry]", V1, V2)
{
  // The local AttributeFormat enum and halp::attribute_format agree for
  // values 0..7 (Float4..UInt4); each publishes the right format and
  // tightly-packed stride. (From UInt2 upward they diverge — see the
  // [!shouldfail] defect test below.)
  struct Case
  {
    Threedim::AttributeFormat in;
    halp::attribute_format out;
    int packed_stride;
  };
  const Case cases[] = {
      {Threedim::AttributeFormat::Float4, halp::attribute_format::float4, 16},
      {Threedim::AttributeFormat::Float2, halp::attribute_format::float2, 8},
      {Threedim::AttributeFormat::Float, halp::attribute_format::float1, 4},
      {Threedim::AttributeFormat::UNormByte4, halp::attribute_format::unormbyte4, 4},
      {Threedim::AttributeFormat::UInt4, halp::attribute_format::uint4, 16},
  };
  for(const auto& c : cases)
  {
    TestType node;
    setBuffer(node, 0, handle_a, 64);
    setAttrCommon(node, 0, 0, 0, /*stride*/ 0, c.in, false);
    node.inputs.vertices.value = 2;
    node();
    auto& mesh = node.outputs.geometry.mesh;
    REQUIRE(mesh.attributes.size() == 1);
    CHECK(mesh.attributes[0].format == c.out);
    REQUIRE(mesh.bindings.size() == 1);
    CHECK(mesh.bindings[0].stride == c.packed_stride);
  }
}

// DEFECT 1: BufferToGeometryCommon.hpp's toHalpFormat and attributeFormatSize
// both static_cast the local AttributeFormat enum straight into
// halp::attribute_format. The local enum (Float4..SShort, 0..22, commented
// "Matches QRhiVertexInputAttribute::Format") has NO 3-component int/short
// entries, while halp::attribute_format interleaves uint3/sint3/ushort3/
// sshort3 into the sequence. The two enums therefore agree only for values
// 0..7 (Float4..UInt4); from UInt2 (8) upward every cast lands on the wrong
// halp entry: UInt2->uint3, UInt->uint2, SInt4->uint1, SInt2->sint4,
// Half4->sint2, UShort4->half2, SShort4->ushort3, ... The published
// descriptor then carries both a wrong vertex-input format AND a wrong
// tightly-packed stride (UInt2 should be uint2 / 8 bytes; it ships as
// uint3 / 12 bytes). Correct expectations asserted; flips red-to-green the
// day the mapping is fixed (e.g. by a real switch instead of the cast).
TEMPLATE_TEST_CASE(
    "AttributeFormat::UInt2 must publish halp uint2 with an 8-byte packed stride",
    "[threedim][buffertogeometry][!shouldfail]", V1, V2)
{
  TestType node;
  setBuffer(node, 0, handle_a, 64);
  setAttrCommon(node, 0, 0, 0, /*stride*/ 0, Threedim::AttributeFormat::UInt2, false);
  node.inputs.vertices.value = 2;
  node();

  auto& mesh = node.outputs.geometry.mesh;
  REQUIRE(mesh.attributes.size() == 1);
  CHECK(mesh.attributes[0].format == halp::attribute_format::uint2); // ships uint3
  REQUIRE(mesh.bindings.size() == 1);
  CHECK(mesh.bindings[0].stride == 8); // ships 12
}

// DEFECT 2: the "null buffer somewhere" early return in operator() wipes the
// descriptor (mesh.buffers/bindings/attributes/input are cleared before the
// first pass detects the null handle) but returns WITHOUT raising dirty_mesh.
// After a preceding no-change tick left dirty_mesh == false, the wipe ships
// with dirty_mesh == false, so downstream keeps its cached GPU geometry —
// which still references the old, possibly freed, buffer handle (the exact
// dangling-handle class InstancerStaleBuffer guards against). The output
// mesh changed, so dirty_mesh must be raised.
TEMPLATE_TEST_CASE(
    "wiping the descriptor on a null handle must raise dirty_mesh",
    "[threedim][buffertogeometry][!shouldfail]", V1, V2)
{
  TestType node;
  setBuffer(node, 0, handle_a, 48);
  setAttrCommon(node, 0, 0, 0, 0, Threedim::AttributeFormat::Float3, false);
  node.inputs.vertices.value = 4;
  node();
  REQUIRE(node.outputs.geometry.mesh.buffers.size() == 1);

  // Intermediate no-change tick parks dirty_mesh at false.
  node();
  REQUIRE(!node.outputs.geometry.dirty_mesh);

  // The buffer disappears: the descriptor is wiped...
  node.inputs.buffer_0.buffer.handle = nullptr;
  node();
  REQUIRE(node.outputs.geometry.mesh.buffers.empty());
  // ...so the change must be announced. Currently stays false.
  CHECK(node.outputs.geometry.dirty_mesh);
}

// -----------------------------------------------------------------------------
// v1-specific: the location spinbox -> semantic table.
// -----------------------------------------------------------------------------

TEST_CASE(
    "BuffersToGeometry v1 maps the location control onto fixed semantics",
    "[threedim][buffertogeometry]")
{
  struct Case
  {
    int location;
    bool instanced;
    halp::attribute_semantic expected;
  };
  const Case cases[] = {
      {0, false, halp::attribute_semantic::position},
      {1, false, halp::attribute_semantic::texcoord0},
      {2, false, halp::attribute_semantic::color0},
      {3, false, halp::attribute_semantic::normal},
      {4, false, halp::attribute_semantic::tangent},
      {7, false, halp::attribute_semantic::custom},
      // Instanced remaps the transform slots:
      {0, true, halp::attribute_semantic::translation},
      {3, true, halp::attribute_semantic::rotation},
      {4, true, halp::attribute_semantic::scale},
  };
  for(const auto& c : cases)
  {
    V1 node;
    setBuffer(node, 0, handle_a, 64);
    setAttr(node, 0, 0, 0, 0, Threedim::AttributeFormat::Float4, c.location,
            c.instanced);
    node.inputs.vertices.value = 4;
    node();
    auto& mesh = node.outputs.geometry.mesh;
    REQUIRE(mesh.attributes.size() == 1);
    CHECK(mesh.attributes[0].semantic == c.expected);
  }
}

// -----------------------------------------------------------------------------
// v2-specific: the free-text semantic name -> ossia::name_to_semantic.
// -----------------------------------------------------------------------------

TEST_CASE(
    "BuffersToGeometry2 resolves semantic names case-insensitively, with "
    "aliases; unknown and empty resolve to custom",
    "[threedim][buffertogeometry]")
{
  struct Case
  {
    const char* name;
    halp::attribute_semantic expected;
  };
  const Case cases[] = {
      {"position", halp::attribute_semantic::position},
      {"Normal", halp::attribute_semantic::normal}, // case-insensitive
      {"uv", halp::attribute_semantic::texcoord0},  // alias
      {"tangent", halp::attribute_semantic::tangent},
      {"", halp::attribute_semantic::custom},
      {"frobnicate", halp::attribute_semantic::custom},
  };
  for(const auto& c : cases)
  {
    V2 node;
    setBuffer(node, 0, handle_a, 64);
    setAttr(node, 0, 0, 0, 0, Threedim::AttributeFormat::Float3, c.name);
    node.inputs.vertices.value = 4;
    node();
    auto& mesh = node.outputs.geometry.mesh;
    REQUIRE(mesh.attributes.size() == 1);
    CHECK(mesh.attributes[0].semantic == c.expected);
  }
}

TEST_CASE(
    "BuffersToGeometry2: editing only the semantic string triggers a rebuild",
    "[threedim][buffertogeometry]")
{
  V2 node;
  setBuffer(node, 0, handle_a, 48);
  setAttr(node, 0, 0, 0, 0, Threedim::AttributeFormat::Float3, "position");
  node.inputs.vertices.value = 4;
  node();
  REQUIRE(
      node.outputs.geometry.mesh.attributes[0].semantic
      == halp::attribute_semantic::position);

  node();
  REQUIRE(!node.outputs.geometry.dirty_mesh);

  node.inputs.semantic_0.value = "normal";
  node();
  CHECK(node.outputs.geometry.dirty_mesh);
  REQUIRE(node.outputs.geometry.mesh.attributes.size() == 1);
  CHECK(
      node.outputs.geometry.mesh.attributes[0].semantic
      == halp::attribute_semantic::normal);
}
