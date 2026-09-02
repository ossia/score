// GeometryPacker's CPU-side packing contract. The actual byte shuffling runs
// in a compute shader, but the LAYOUT it executes — interleave order, output
// stride, per-attribute output offsets, vec4 padding, source-location
// arithmetic (attribute byte_offset + input byte_offset), index-buffer
// passthrough and the skip-on-missing-attribute policy — is decided entirely
// on the CPU in PackedExtractionStrategy::init(), and published through the
// public outputStride()/attributeCount()/attributeInfo() accessors.
//
// GPU-free seam: with mesh.vertices == 0, init() computes the complete layout
// and then returns false at its documented "Zero output size" guard — which
// sits BEFORE the first use of either the QRhi& (rhi.newBuffer) or the
// RenderState& (createPipeline). Every rejection path we exercise (bad spec
// count, no matching attributes, invalid index buffer) also precedes any GPU
// work. update() with an unchanged zero size likewise touches neither (the
// resize branch is skipped and updateBindings() bails on the null SRB). So —
// as in CameraRelease.cpp — we hand init()/update() references into inert
// storage they are contractually forbidden to dereference.
//
// buildAttributeSpecs() / specsChanged() (the node-level half that fixes the
// position→normal→color→texcoord→tangent interleave order) are private; as in
// ExtractIndex.cpp we expose them with `#define private public` scoped to the
// GeometryPacker.hpp include only, pre-including everything else first.
//
// Missing-attribute policy, derived from source: a spec whose semantic is not
// found in the mesh hits `continue` in init()'s spec loop — the attribute is
// SKIPPED and later attributes close the gap; nothing is zero-filled. (The
// only zero-fill in the design is the shader-side vec4 padding, lanes filled
// with 0 and w = 1.0f / 0x3f800000 — GPU-side, out of scope here.)

#include <catch2/catch_test_macros.hpp>

#include <Threedim/GeometryToBufferStrategies.hpp>
#include <halp/buffer.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>

#include <array>
#include <cstdint>
#include <new>
#include <span>
#include <vector>

#define private public
#include <Threedim/GeometryPacker.hpp>
#undef private

using halp::attribute_format;
using halp::attribute_semantic;
using Threedim::packed_attribute_spec;

namespace
{
// Inert, correctly-aligned storage for the two references init()/update()
// must never dereference on the paths under test (see top comment).
score::gfx::RenderState& inertRenderState()
{
  alignas(std::max_align_t) static unsigned char storage[1024]{};
  return *reinterpret_cast<score::gfx::RenderState*>(&storage[0]);
}

QRhi& inertRhi()
{
  alignas(std::max_align_t) static unsigned char storage[1024]{};
  return *reinterpret_cast<QRhi*>(&storage[0]);
}

// Append one tightly-packed, single-attribute stream (its own buffer, binding
// and input) to the mesh — the shape primitive generators emit.
void addStream(
    halp::dynamic_gpu_geometry& mesh, attribute_semantic sem, attribute_format fmt,
    int stride, void* handle)
{
  const int idx = static_cast<int>(mesh.buffers.size());
  mesh.buffers.push_back(
      halp::geometry_gpu_buffer{.handle = handle, .byte_size = 4096});
  mesh.bindings.push_back(
      halp::geometry_binding{
          .stride = stride,
          .step_rate = 1,
          .classification = halp::binding_classification::per_vertex});
  mesh.input.push_back(halp::geometry_input{.buffer = idx, .byte_offset = 0});
  mesh.attributes.push_back(
      halp::geometry_attribute{.binding = idx, .semantic = sem, .format = fmt});
}

// pos(float3) + normal(float3) + texcoord(float2), three separate tight
// streams. vertices deliberately 0: the layout is computed, the GPU is not
// touched (see top comment).
halp::dynamic_gpu_geometry makeSeparateStreams()
{
  static int b0, b1, b2;
  halp::dynamic_gpu_geometry mesh;
  addStream(mesh, attribute_semantic::position, attribute_format::float3, 12, &b0);
  addStream(mesh, attribute_semantic::normal, attribute_format::float3, 12, &b1);
  addStream(mesh, attribute_semantic::texcoord0, attribute_format::float2, 8, &b2);
  mesh.vertices = 0;
  return mesh;
}

// One interleaved 32-byte vertex — pos@0, normal@12, uv@24 — in a single
// buffer whose input starts 256 bytes in.
halp::dynamic_gpu_geometry makeInterleaved()
{
  static int b0;
  halp::dynamic_gpu_geometry mesh;
  mesh.buffers.push_back(halp::geometry_gpu_buffer{.handle = &b0, .byte_size = 4096});
  mesh.bindings.push_back(
      halp::geometry_binding{
          .stride = 32,
          .step_rate = 1,
          .classification = halp::binding_classification::per_vertex});
  mesh.input.push_back(halp::geometry_input{.buffer = 0, .byte_offset = 256});
  mesh.attributes.push_back(
      halp::geometry_attribute{
          .binding = 0,
          .semantic = attribute_semantic::position,
          .format = attribute_format::float3,
          .byte_offset = 0});
  mesh.attributes.push_back(
      halp::geometry_attribute{
          .binding = 0,
          .semantic = attribute_semantic::normal,
          .format = attribute_format::float3,
          .byte_offset = 12});
  mesh.attributes.push_back(
      halp::geometry_attribute{
          .binding = 0,
          .semantic = attribute_semantic::texcoord0,
          .format = attribute_format::float2,
          .byte_offset = 24});
  mesh.vertices = 0;
  return mesh;
}

const std::vector<packed_attribute_spec> posNrmUv{
    {.location = attribute_semantic::position, .pad_to_vec4 = false},
    {.location = attribute_semantic::normal, .pad_to_vec4 = false},
    {.location = attribute_semantic::texcoord0, .pad_to_vec4 = false},
};
} // namespace

TEST_CASE(
    "packed layout interleaves three tight streams in spec order",
    "[threedim][geometrypacker]")
{
  auto mesh = makeSeparateStreams();
  Threedim::PackedExtractionStrategy s;

  // False is the documented zero-vertex "Zero output size" refusal; the layout
  // has been fully computed by then.
  CHECK_FALSE(s.init(inertRenderState(), inertRhi(), mesh, posNrmUv));

  CHECK(s.attributeCount() == 3);
  // 3 + 3 + 2 floats = 12 + 12 + 8 bytes.
  CHECK(s.outputStride() == 32);

  const auto& a0 = s.attributeInfo(0); // position
  CHECK(a0.is_active);
  CHECK(a0.src_buffer_index == 0);
  CHECK(a0.src_stride == 12);
  CHECK(a0.src_offset == 0);
  CHECK(a0.element_count == 3);
  CHECK(a0.output_components == 3);
  CHECK(a0.output_offset == 0);
  CHECK(a0.is_float);

  const auto& a1 = s.attributeInfo(1); // normal, right after position
  CHECK(a1.src_buffer_index == 1);
  CHECK(a1.element_count == 3);
  CHECK(a1.output_components == 3);
  CHECK(a1.output_offset == 12);

  const auto& a2 = s.attributeInfo(2); // texcoord, after normal
  CHECK(a2.src_buffer_index == 2);
  CHECK(a2.src_stride == 8);
  CHECK(a2.element_count == 2);
  CHECK(a2.output_components == 2);
  CHECK(a2.output_offset == 24);

  // Slot past attributeCount() stays inactive.
  CHECK_FALSE(s.attributeInfo(3).is_active);

  // Three distinct source buffers were registered.
  CHECK(s.m_srcBufferCount == 3);
}

TEST_CASE(
    "pad_to_vec4 widens float3 lanes to 16 bytes and shifts later offsets",
    "[threedim][geometrypacker]")
{
  auto mesh = makeSeparateStreams();
  const std::vector<packed_attribute_spec> specs{
      {.location = attribute_semantic::position, .pad_to_vec4 = true},
      {.location = attribute_semantic::normal, .pad_to_vec4 = true},
      {.location = attribute_semantic::texcoord0, .pad_to_vec4 = false},
  };

  Threedim::PackedExtractionStrategy s;
  CHECK_FALSE(s.init(inertRenderState(), inertRhi(), mesh, specs));

  CHECK(s.attributeCount() == 3);
  // vec4 pos (16) + vec4 normal (16) + vec2 uv (8).
  CHECK(s.outputStride() == 40);

  CHECK(s.attributeInfo(0).element_count == 3); // source is still float3...
  CHECK(s.attributeInfo(0).output_components == 4); // ...output is vec4
  CHECK(s.attributeInfo(0).output_offset == 0);
  CHECK(s.attributeInfo(1).output_components == 4);
  CHECK(s.attributeInfo(1).output_offset == 16);
  CHECK(s.attributeInfo(2).output_components == 2);
  CHECK(s.attributeInfo(2).output_offset == 32);
  // The padding lanes themselves are shader-side: 0 fill, w = 0x3f800000.
}

TEST_CASE(
    "pad_to_vec4 is refused for integer formats",
    "[threedim][geometrypacker]")
{
  // Silently reinterpreting integer lanes as padded floats would corrupt
  // e.g. joint indices; the strategy only pads float-ish sources.
  static int b0;
  halp::dynamic_gpu_geometry mesh;
  addStream(mesh, attribute_semantic::color0, attribute_format::sint2, 8, &b0);
  mesh.vertices = 0;

  const std::vector<packed_attribute_spec> specs{
      {.location = attribute_semantic::color0, .pad_to_vec4 = true},
  };

  Threedim::PackedExtractionStrategy s;
  CHECK_FALSE(s.init(inertRenderState(), inertRhi(), mesh, specs));

  CHECK(s.attributeCount() == 1);
  CHECK_FALSE(s.attributeInfo(0).is_float);
  CHECK(s.attributeInfo(0).element_count == 2);
  CHECK(s.attributeInfo(0).output_components == 2); // NOT padded
  CHECK(s.outputStride() == 8);
}

TEST_CASE(
    "a spec whose attribute is missing is skipped, later offsets close the gap",
    "[threedim][geometrypacker]")
{
  // Mesh has position + texcoord but NO normal; the spec list asks for all
  // three. Per source (init()'s `continue` on failed lookup) the normal slot
  // is dropped entirely — texcoord lands right after position, and the
  // stride shrinks accordingly. Nothing is zero-filled.
  static int b0, b1;
  halp::dynamic_gpu_geometry mesh;
  addStream(mesh, attribute_semantic::position, attribute_format::float3, 12, &b0);
  addStream(mesh, attribute_semantic::texcoord0, attribute_format::float2, 8, &b1);
  mesh.vertices = 0;

  Threedim::PackedExtractionStrategy s;
  CHECK_FALSE(s.init(inertRenderState(), inertRhi(), mesh, posNrmUv));

  CHECK(s.attributeCount() == 2);
  CHECK(s.outputStride() == 20); // 12 + 8, no 12-byte hole for the normal

  CHECK(s.attributeInfo(0).output_offset == 0); // position
  CHECK(s.attributeInfo(1).element_count == 2); // texcoord...
  CHECK(s.attributeInfo(1).output_offset == 12); // ...immediately after
}

TEST_CASE(
    "interleaved source: src offsets add attribute and input byte offsets",
    "[threedim][geometrypacker]")
{
  auto mesh = makeInterleaved();
  Threedim::PackedExtractionStrategy s;
  CHECK_FALSE(s.init(inertRenderState(), inertRhi(), mesh, posNrmUv));

  CHECK(s.attributeCount() == 3);
  CHECK(s.outputStride() == 32);

  for(int i = 0; i < 3; ++i)
  {
    CHECK(s.attributeInfo(i).src_buffer_index == 0);
    CHECK(s.attributeInfo(i).src_stride == 32);
  }
  // input.byte_offset (256) + attribute.byte_offset (0 / 12 / 24).
  CHECK(s.attributeInfo(0).src_offset == 256);
  CHECK(s.attributeInfo(1).src_offset == 268);
  CHECK(s.attributeInfo(2).src_offset == 280);

  CHECK(s.attributeInfo(0).output_offset == 0);
  CHECK(s.attributeInfo(1).output_offset == 12);
  CHECK(s.attributeInfo(2).output_offset == 24);

  // One shared source buffer, registered once.
  CHECK(s.m_srcBufferCount == 1);
}

TEST_CASE("index buffer configuration passes through", "[threedim][geometrypacker]")
{
  SECTION("uint16 index buffer: handle, offset and 16-bit flag")
  {
    static int idxbuf;
    auto mesh = makeInterleaved();
    mesh.buffers.push_back(
        halp::geometry_gpu_buffer{.handle = &idxbuf, .byte_size = 4096});
    mesh.index.buffer = 1;
    mesh.index.byte_offset = 128;
    mesh.index.format = halp::index_format::uint16;

    Threedim::PackedExtractionStrategy s;
    CHECK_FALSE(s.init(inertRenderState(), inertRhi(), mesh, posNrmUv));

    CHECK(s.m_hasIndexBuffer);
    CHECK(static_cast<void*>(s.m_indexBuffer) == static_cast<void*>(&idxbuf));
    CHECK(s.m_indexOffset == 128);
    CHECK_FALSE(s.m_indexFormat32);
  }

  SECTION("uint32 index buffer sets the 32-bit flag")
  {
    static int idxbuf;
    auto mesh = makeInterleaved();
    mesh.buffers.push_back(
        halp::geometry_gpu_buffer{.handle = &idxbuf, .byte_size = 4096});
    mesh.index.buffer = 1;
    mesh.index.byte_offset = 0;
    mesh.index.format = halp::index_format::uint32;

    Threedim::PackedExtractionStrategy s;
    CHECK_FALSE(s.init(inertRenderState(), inertRhi(), mesh, posNrmUv));
    CHECK(s.m_hasIndexBuffer);
    CHECK(s.m_indexFormat32);
  }

  SECTION("non-indexed mesh (buffer = -1)")
  {
    auto mesh = makeInterleaved();
    REQUIRE(mesh.index.buffer == -1);
    Threedim::PackedExtractionStrategy s;
    CHECK_FALSE(s.init(inertRenderState(), inertRhi(), mesh, posNrmUv));
    CHECK_FALSE(s.m_hasIndexBuffer);
  }

  SECTION("index pointing outside the buffers array is refused")
  {
    auto mesh = makeInterleaved();
    mesh.index.buffer = 7; // only 1 buffer exists
    Threedim::PackedExtractionStrategy s;
    CHECK_FALSE(s.init(inertRenderState(), inertRhi(), mesh, posNrmUv));
    // Refused at the "Invalid index buffer" guard: nothing configured.
    CHECK(s.m_indexBuffer == nullptr);
  }
}

TEST_CASE(
    "init refuses bad spec lists before any GPU work",
    "[threedim][geometrypacker]")
{
  auto mesh = makeSeparateStreams();

  SECTION("empty spec list")
  {
    Threedim::PackedExtractionStrategy s;
    CHECK_FALSE(
        s.init(
            inertRenderState(), inertRhi(), mesh,
            std::span<const packed_attribute_spec>{}));
    CHECK(s.attributeCount() == 0);
  }

  SECTION("more than MAX_PACKED_ATTRIBUTES (8) specs")
  {
    const std::vector<packed_attribute_spec> nine(
        9, packed_attribute_spec{
               .location = attribute_semantic::position, .pad_to_vec4 = false});
    Threedim::PackedExtractionStrategy s;
    CHECK_FALSE(s.init(inertRenderState(), inertRhi(), mesh, nine));
    CHECK(s.attributeCount() == 0);
  }

  SECTION("no requested attribute exists in the mesh")
  {
    const std::vector<packed_attribute_spec> specs{
        {.location = attribute_semantic::joints0, .pad_to_vec4 = false},
        {.location = attribute_semantic::tangent, .pad_to_vec4 = false},
    };
    Threedim::PackedExtractionStrategy s;
    CHECK_FALSE(s.init(inertRenderState(), inertRhi(), mesh, specs));
    CHECK(s.attributeCount() == 0);
    CHECK(s.outputStride() == 0);
  }
}

TEST_CASE(
    "update refreshes source locations but never the packed layout",
    "[threedim][geometrypacker]")
{
  // A producer re-uploading into a different arena region changes the input
  // byte_offset; update() must chase it. The output layout (offsets, stride)
  // is fixed at init() — update() intentionally leaves it alone.
  auto mesh = makeInterleaved();
  Threedim::PackedExtractionStrategy s;
  CHECK_FALSE(s.init(inertRenderState(), inertRhi(), mesh, posNrmUv));
  REQUIRE(s.attributeCount() == 3);

  mesh.input[0].byte_offset = 512;
  s.update(inertRhi(), mesh, posNrmUv);

  CHECK(s.attributeInfo(0).src_offset == 512);
  CHECK(s.attributeInfo(1).src_offset == 524);
  CHECK(s.attributeInfo(2).src_offset == 536);

  // Layout untouched.
  CHECK(s.outputStride() == 32);
  CHECK(s.attributeInfo(0).output_offset == 0);
  CHECK(s.attributeInfo(1).output_offset == 12);
  CHECK(s.attributeInfo(2).output_offset == 24);
}

TEST_CASE(
    "buildAttributeSpecs emits the fixed position/normal/color/texcoord/tangent "
    "order and maps Vec4 to pad_to_vec4",
    "[threedim][geometrypacker]")
{
  using GP = Threedim::GeometryPacker;
  GP node;
  node.inputs.position.value = GP::Attribute3::Vec4;
  node.inputs.normal.value = GP::Attribute3::Vec3;
  node.inputs.color.value = GP::Attribute3::None; // disabled -> omitted
  node.inputs.texcoord.value = GP::Attribute2::Vec2;
  node.inputs.tangent.value = GP::Attribute3::Vec3;

  node.buildAttributeSpecs();

  REQUIRE(node.m_specs.size() == 4);
  CHECK(node.m_specs[0].location == attribute_semantic::position);
  CHECK(node.m_specs[0].pad_to_vec4); // Vec4 choice pads
  CHECK(node.m_specs[1].location == attribute_semantic::normal);
  CHECK_FALSE(node.m_specs[1].pad_to_vec4);
  CHECK(node.m_specs[2].location == attribute_semantic::texcoord0);
  CHECK_FALSE(node.m_specs[2].pad_to_vec4);
  CHECK(node.m_specs[3].location == attribute_semantic::tangent);
  CHECK_FALSE(node.m_specs[3].pad_to_vec4);
}

TEST_CASE(
    "specsChanged detects count and padding differences",
    "[threedim][geometrypacker]")
{
  using GP = Threedim::GeometryPacker;
  GP node;
  node.inputs.position.value = GP::Attribute3::Vec4;
  node.inputs.texcoord.value = GP::Attribute2::Vec2;
  node.buildAttributeSpecs();
  node.m_lastSpecs = node.m_specs;
  CHECK_FALSE(node.specsChanged());

  SECTION("dropping an attribute changes the specs")
  {
    node.inputs.texcoord.value = GP::Attribute2::None;
    node.buildAttributeSpecs();
    CHECK(node.specsChanged());
  }

  SECTION("changing only the padding changes the specs")
  {
    node.inputs.position.value = GP::Attribute3::Vec3;
    node.buildAttributeSpecs();
    CHECK(node.specsChanged());
  }
}

TEST_CASE(
    "node-built specs drive the strategy to the expected packed layout",
    "[threedim][geometrypacker]")
{
  // Position=Vec4, Normal=Vec3, TexCoord=Vec2, Tangent=Vec3 over four tight
  // float streams: 16 + 12 + 8 + 12 = 48-byte vertices at offsets 0/16/28/36.
  using GP = Threedim::GeometryPacker;
  GP node;
  node.inputs.position.value = GP::Attribute3::Vec4;
  node.inputs.normal.value = GP::Attribute3::Vec3;
  node.inputs.texcoord.value = GP::Attribute2::Vec2;
  node.inputs.tangent.value = GP::Attribute3::Vec3;
  node.buildAttributeSpecs();
  REQUIRE(node.m_specs.size() == 4);

  static int b0, b1, b2, b3;
  halp::dynamic_gpu_geometry mesh;
  addStream(mesh, attribute_semantic::position, attribute_format::float3, 12, &b0);
  addStream(mesh, attribute_semantic::normal, attribute_format::float3, 12, &b1);
  addStream(mesh, attribute_semantic::texcoord0, attribute_format::float2, 8, &b2);
  addStream(mesh, attribute_semantic::tangent, attribute_format::float3, 12, &b3);
  mesh.vertices = 0;

  Threedim::PackedExtractionStrategy s;
  CHECK_FALSE(s.init(inertRenderState(), inertRhi(), mesh, node.m_specs));

  CHECK(s.attributeCount() == 4);
  CHECK(s.outputStride() == 48);
  CHECK(s.attributeInfo(0).output_offset == 0);
  CHECK(s.attributeInfo(0).output_components == 4); // padded position
  CHECK(s.attributeInfo(1).output_offset == 16);
  CHECK(s.attributeInfo(2).output_offset == 28);
  CHECK(s.attributeInfo(3).output_offset == 36);
}
