#pragma once
#include <Threedim/GeometryToBufferStrategies.hpp>
#include <halp/buffer.hpp>

namespace Threedim
{
class ExtractBuffer
{
public:
  halp_meta(name, "Extract buffer")
  halp_meta(category, "Visuals/Utilities")
  halp_meta(c_name, "extract_attribute")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/extract-buffer.html")
  halp_meta(uuid, "207ab744-1f3e-4e72-9a77-db6017cf3dd5")

  enum Attribute
  {
    Position,
    TexCoord,
    Color,
    Normal,
    Tangent,

    Attribute_0,
    Attribute_1,
    Attribute_2,
    Attribute_3,
    Attribute_4,
    Attribute_5,
    Attribute_6,
    Attribute_7,
    Attribute_8,

    Index,

    Buffer_0,
    Buffer_1,
    Buffer_2,
    Buffer_3,
    Buffer_4,
    Buffer_5,
    Buffer_6,
    Buffer_7,
    Buffer_8
  };

  struct ins
  {
    struct
    {
      halp_meta(name, "Geometry");
      halp::dynamic_gpu_geometry mesh;
      float transform[16]{};
      bool dirty_mesh = false;
      bool dirty_transform = false;
    } geometry;

    halp::combobox_t<"Attribute", Attribute> attribute;
    halp::toggle<"Pad vec3 to vec4"> pad_to_vec4;
  } inputs;

  struct
  {
    halp::gpu_buffer_output<"Buffer"> buffer;
  } outputs;

  ExtractBuffer();

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res);

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e);

  void release(score::gfx::RenderList& r);

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge);

  void operator()();

private:
  [[nodiscard]] static halp::attribute_location
  toAttributeLocation(Attribute attr) noexcept;
  void updateOutput();

  ExtractionStrategyVariant m_strategy;
  Attribute m_currentAttribute{Position};
  ossia::geometry::gpu_buffer m_currentBuffer{};
  bool m_currentPadToVec4{false};
};
}
