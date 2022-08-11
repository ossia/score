#pragma once
/*
#include <Gfx/Graph/Mesh.hpp>

#include <avnd/concepts/gfx.hpp>

namespace avnd
{
}
namespace Crousti
{
template<avnd::geometry_port T>
struct MeshMapper : public score::gfx::Mesh
{
public:
  T geom;

  explicit MeshMapper(T& geometry)
      : geom{geometry}
  {
    avnd::for_each_field_ref(
        typename T::bindings{},
        [this] (auto& binding) {
          QRhiVertexInputBinding b{binding.stride};
          if constexpr(requires { binding.per_instance; })
            b.setClassification(QRhiVertexInputBinding::PerInstance);
          if constexpr(requires { binding.step_rate; })
            b.setInstanceStepRate(binding.step_rate);
          vertexBindings.push_back(b);
    });

    int k = 0;
    avnd::for_each_field_ref(
        typename T::attributes{},
        [this, &k] <typename A> (A& attr) {
          QRhiVertexInputAttribute a{};
          a.setLocation(k++);

          if constexpr(requires { attr.binding; })
            a.setBinding(attr.binding);

          if constexpr(requires { attr.offset; })
            a.setOffset(attr.offset);

          using tp = typename A::datatype;
          if constexpr(std::is_same_v<tp, float>)
            a.setFormt(QRhiVertexInputAttribute::Float);
          else if constexpr(std::is_same_v<tp, float[2]>)
            a.setFormt(QRhiVertexInputAttribute::Float2);
          else if constexpr(std::is_same_v<tp, float[3]>)
            a.setFormt(QRhiVertexInputAttribute::Float3);
          else if constexpr(std::is_same_v<tp, float[4]>)
            a.setFormt(QRhiVertexInputAttribute::Float4);
          else if constexpr(std::is_same_v<tp, uint8_t> || std::is_same_v<tp, unsigned char>)
            a.setFormt(QRhiVertexInputAttribute::UNormByte);
          else if constexpr(std::is_same_v<tp, uint16_t>)
            a.setFormt(QRhiVertexInputAttribute::UNormByte2);
          else if constexpr(std::is_same_v<tp, uint32_t>)
            a.setFormt(QRhiVertexInputAttribute::UNormByte4);
        });
  }

  void update(tcb::span<const float> vtx, int count)
  {
    vertexArray = vtx;
    vertexCount = count;
  }

  void setupBindings(QRhiBuffer& vtxData, QRhiBuffer* idxData, QRhiCommandBuffer& cb) const noexcept override
  {
    constexpr auto sz = avnd::pfr::tuple_size<decltype(T{}.vertex_input)>{};

    QRhiCommandBuffer::VertexInput bindings[sz];

    int i = 0;
    avnd::for_each_field_ref(
        geom.vertex_input,
        [&] (auto& vi) {
          bindings[i++] = { &vtxData, vi.offset };
        });

    // TODO index
    cb.setVertexInput(0, sz, bindings);
  }

  const char* defaultVertexShader() const noexcept override
  {
    return "";
  }
};

}

*/
