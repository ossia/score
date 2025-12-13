#pragma once
#include <Threedim/TinyObj.hpp>
#include <halp/buffer.hpp>
#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>

namespace Threedim
{

class BuffersToGeometry
{
public:
  halp_meta(name, "Buffers to geometry")
  halp_meta(category, "Visuals/3D")
  halp_meta(c_name, "buffers_to_geometry")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/buffers-to-geometry.html")
  halp_meta(uuid, "d5dd3b9a-f57b-4546-9890-d5b5e351dcea")

  // Matches QRhiVertexInputAttribute::Format
  enum AttributeFormat
  {
    Float4,
    Float3,
    Float2,
    Float,
    UNormByte4,
    UNormByte2,
    UNormByte,
    UInt4,
    UInt2,
    UInt,
    SInt4,
    SInt2,
    SInt,
    Half4,
    Half3,
    Half2,
    Half,
    UShort4,
    UShort2,
    UShort,
    SShort4,
    SShort2,
    SShort,
  };

  enum PrimitiveTopology
  {
    Triangles,
    TriangleStrip,
    TriangleFan,
    Lines,
    LineStrip,
    Points
  };

  enum CullMode
  {
    None,
    Front,
    Back
  };

  enum FrontFace
  {
    CounterClockwise,
    Clockwise
  };

  enum IndexFormat
  {
    UInt16,
    UInt32
  };

  struct ins
  {
    // Input buffers
    halp::gpu_buffer_input<"Buffer 0"> buffer_0;
    halp::gpu_buffer_input<"Buffer 1"> buffer_1;
    halp::gpu_buffer_input<"Buffer 2"> buffer_2;
    halp::gpu_buffer_input<"Buffer 3"> buffer_3;
    halp::gpu_buffer_input<"Buffer 4"> buffer_4;
    halp::gpu_buffer_input<"Buffer 5"> buffer_5;
    halp::gpu_buffer_input<"Buffer 6"> buffer_6;
    halp::gpu_buffer_input<"Buffer 7"> buffer_7;

    // clang-format off
#define ATTRIBUTE(i) \
        halp::spinbox_i32<"Attr" #i " buffer", halp::irange{-1, 7, 0}> attribute_buffer_ ## i; \
        halp::spinbox_i32<"Attr" #i " offset", halp::irange{0, 1000000, 0}> attribute_offset_ ## i; \
        halp::spinbox_i32<"Attr" #i " stride", halp::irange{0, 1024, 0}> attribute_stride_ ## i; \
        halp::combobox_t<"Attr" #i " format", AttributeFormat> format_ ## i; \
        halp::spinbox_i32<"Attr" #i " location", halp::irange{0, 15, i}> location_ ## i; \
        halp::toggle<"Attr" #i " instanced"> instanced_ ## i;
    
    ATTRIBUTE(0)
    ATTRIBUTE(1)
    ATTRIBUTE(2)
    ATTRIBUTE(3)
    ATTRIBUTE(4)
    ATTRIBUTE(5)
    ATTRIBUTE(6)
    ATTRIBUTE(7)
#undef ATTRIBUTE
    // clang-format on

    // Index buffer
    halp::spinbox_i32<"Index Buffer", halp::irange{-1, 7, 0}> index_buffer;
    halp::combobox_t<"Index Format", IndexFormat> index_format;
    halp::spinbox_i32<"Index Offset", halp::irange{0, 1000000, 0}> index_offset;

    // Geometry settings
    halp::spinbox_i32<"Vertices", halp::irange{0, 1000000000, 0}> vertices;
    halp::spinbox_i32<"Instances", halp::irange{1, 1000000000, 1}> instances;
    halp::combobox_t<"Topology", PrimitiveTopology> topology;
    halp::combobox_t<"Cull Mode", CullMode> cull_mode;
    halp::combobox_t<"Front Face", FrontFace> front_face;

    // Transform
    PositionControl position;
    RotationControl rotation;
    ScaleControl scale;
  } inputs;

  struct
  {
    struct
    {
      halp_meta(name, "Geometry");
      halp::dynamic_gpu_geometry mesh;
      float transform[16]{};
      bool dirty_mesh = false;
      bool dirty_transform = false;
    } geometry;
  } outputs;

  BuffersToGeometry();
  void operator()();

  // Cache previous state to detect changes
  struct AttributeState
  {
    bool enabled{};
    int32_t buffer{};
    int32_t offset{};
    int32_t stride{};
    AttributeFormat format{};
    int32_t location{};
    bool instanced{};
  };

  std::array<AttributeState, 8> m_prevAttributes{};
  int32_t m_prevUseIndexBuffer{};
  IndexFormat m_prevIndexFormat{};
  int32_t m_prevIndexOffset{};
  int32_t m_prevVertices{};
  PrimitiveTopology m_prevTopology{};
  CullMode m_prevCullMode{};
  FrontFace m_prevFrontFace{};

  struct ui
  {
    halp_meta(name, "Main")
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::background_mid)

    struct T
    {
      halp_meta(name, "Tabs")
      halp_meta(layout, halp::layouts::tabs)
      halp_meta(background, halp::colors::background_darker)
      struct G
      {
        halp_meta(name, "Global")
        halp_meta(layout, halp::layouts::hbox)
        struct A
        {
          halp_meta(layout, halp::layouts::vbox)
          halp::item<&ins::vertices> vertices;
          halp::item<&ins::instances> instances;
          halp::item<&ins::topology> topology;
          halp::item<&ins::cull_mode> cull_mode;
          halp::item<&ins::front_face> front_face;
        } a;

        struct B
        {
          halp_meta(layout, halp::layouts::vbox)
          halp::item<&ins::position> p;
          halp::item<&ins::rotation> r;
          halp::item<&ins::scale> s;
        } b;
      } global;

      struct A1
      {
        halp_meta(name, "0")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::attribute_buffer_0> buffer;
        halp::item<&ins::attribute_offset_0> offset;
        halp::item<&ins::attribute_stride_0> stride;
        halp::item<&ins::format_0> format;
        halp::item<&ins::location_0> location;
        halp::item<&ins::instanced_0> instanced;
      } a0;
      struct A2
      {
        halp_meta(name, "1")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::attribute_buffer_1> buffer;
        halp::item<&ins::attribute_offset_1> offset;
        halp::item<&ins::attribute_stride_1> stride;
        halp::item<&ins::format_1> format;
        halp::item<&ins::location_1> location;
        halp::item<&ins::instanced_1> instanced;
      } a1;
      struct
      {
        halp_meta(name, "2")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::attribute_buffer_2> buffer;
        halp::item<&ins::attribute_offset_2> offset;
        halp::item<&ins::attribute_stride_2> stride;
        halp::item<&ins::format_2> format;
        halp::item<&ins::location_2> location;
        halp::item<&ins::instanced_2> instanced;
      } a2;
      struct A3
      {
        halp_meta(name, "3")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::attribute_buffer_3> buffer;
        halp::item<&ins::attribute_offset_3> offset;
        halp::item<&ins::attribute_stride_3> stride;
        halp::item<&ins::format_3> format;
        halp::item<&ins::location_3> location;
        halp::item<&ins::instanced_3> instanced;
      } a3;
      struct A4
      {
        halp_meta(name, "4")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::attribute_buffer_4> buffer;
        halp::item<&ins::attribute_offset_4> offset;
        halp::item<&ins::attribute_stride_4> stride;
        halp::item<&ins::format_4> format;
        halp::item<&ins::location_4> location;
        halp::item<&ins::instanced_4> instanced;
      } a4;
      struct A5
      {
        halp_meta(name, "5")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::attribute_buffer_5> buffer;
        halp::item<&ins::attribute_offset_5> offset;
        halp::item<&ins::attribute_stride_5> stride;
        halp::item<&ins::format_5> format;
        halp::item<&ins::location_5> location;
        halp::item<&ins::instanced_5> instanced;
      } a5;
      struct A6
      {
        halp_meta(name, "6")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::attribute_buffer_6> buffer;
        halp::item<&ins::attribute_offset_6> offset;
        halp::item<&ins::attribute_stride_6> stride;
        halp::item<&ins::format_6> format;
        halp::item<&ins::location_6> location;
        halp::item<&ins::instanced_6> instanced;
      } a6;
      struct A7
      {
        halp_meta(name, "7")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::attribute_buffer_7> buffer;
        halp::item<&ins::attribute_offset_7> offset;
        halp::item<&ins::attribute_stride_7> stride;
        halp::item<&ins::format_7> format;
        halp::item<&ins::location_7> location;
        halp::item<&ins::instanced_7> instanced;
      } a7;
      struct
      {
        halp_meta(name, "Index")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::index_buffer> buffer;
        halp::item<&ins::index_offset> offset;
        halp::item<&ins::index_format> format;
      } index;
    } tabs;
  };
};

} // namespace Threedim
