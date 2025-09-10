#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>

#include <Gfx/CommandFactory.hpp>
#include <Gfx/Graph/BufferGeometryNode.hpp>
#include <Gfx/BufferGeometry/Metadata.hpp>
#include <Gfx/TexturePort.hpp>

#include <score/command/PropertyCommand.hpp>

namespace Gfx::BufferGeometry
{
class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Gfx::BufferGeometry::Model)
  W_OBJECT(Model)

public:
  constexpr bool hasExternalUI() { return false; }
  Model(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent);

  template <typename Impl>
  Model(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  ~Model() override;

  // Configuration access for the UI
  score::gfx::BufferGeometryNode::Configuration& configuration() noexcept { return m_config; }
  const score::gfx::BufferGeometryNode::Configuration& configuration() const noexcept { return m_config; }
  
  void configurationChanged() W_SIGNAL(configurationChanged);

  // Vertex count property
  int vertexCount() const noexcept { return m_config.vertex_count; }
  void setVertexCount(int count);
  void vertexCountChanged(int count) W_SIGNAL(vertexCountChanged, count);
  PROPERTY(int, vertexCount READ vertexCount WRITE setVertexCount NOTIFY vertexCountChanged)

  // Position attribute properties
  int positionLocation() const noexcept { return m_config.position.location; }
  void setPositionLocation(int loc);
  void positionLocationChanged(int loc) W_SIGNAL(positionLocationChanged, loc);
  
  int positionFormat() const noexcept { return m_config.position.format; }
  void setPositionFormat(int fmt);
  void positionFormatChanged(int fmt) W_SIGNAL(positionFormatChanged, fmt);
  
  int positionOffset() const noexcept { return static_cast<int>(m_config.position.offset); }
  void setPositionOffset(int offset);
  void positionOffsetChanged(int offset) W_SIGNAL(positionOffsetChanged, offset);
  
  int positionStride() const noexcept { return static_cast<int>(m_config.position.stride); }
  void setPositionStride(int stride);
  void positionStrideChanged(int stride) W_SIGNAL(positionStrideChanged, stride);
  
  int positionBuffer() const noexcept { return m_config.position.buffer_index; }
  void setPositionBuffer(int buf);
  void positionBufferChanged(int buf) W_SIGNAL(positionBufferChanged, buf);

  // Normal attribute properties  
  int normalLocation() const noexcept { return m_config.normal.location; }
  void setNormalLocation(int loc);
  void normalLocationChanged(int loc) W_SIGNAL(normalLocationChanged, loc);
  
  int normalFormat() const noexcept { return m_config.normal.format; }
  void setNormalFormat(int fmt);
  void normalFormatChanged(int fmt) W_SIGNAL(normalFormatChanged, fmt);
  
  int normalOffset() const noexcept { return static_cast<int>(m_config.normal.offset); }
  void setNormalOffset(int offset);
  void normalOffsetChanged(int offset) W_SIGNAL(normalOffsetChanged, offset);
  
  int normalStride() const noexcept { return static_cast<int>(m_config.normal.stride); }
  void setNormalStride(int stride);
  void normalStrideChanged(int stride) W_SIGNAL(normalStrideChanged, stride);
  
  int normalBuffer() const noexcept { return m_config.normal.buffer_index; }
  void setNormalBuffer(int buf);
  void normalBufferChanged(int buf) W_SIGNAL(normalBufferChanged, buf);

  // TexCoord0 attribute properties
  int texcoord0Location() const noexcept { return m_config.texcoord0.location; }
  void setTexcoord0Location(int loc);
  void texcoord0LocationChanged(int loc) W_SIGNAL(texcoord0LocationChanged, loc);
  
  int texcoord0Format() const noexcept { return m_config.texcoord0.format; }
  void setTexcoord0Format(int fmt);
  void texcoord0FormatChanged(int fmt) W_SIGNAL(texcoord0FormatChanged, fmt);
  
  int texcoord0Offset() const noexcept { return static_cast<int>(m_config.texcoord0.offset); }
  void setTexcoord0Offset(int offset);
  void texcoord0OffsetChanged(int offset) W_SIGNAL(texcoord0OffsetChanged, offset);
  
  int texcoord0Stride() const noexcept { return static_cast<int>(m_config.texcoord0.stride); }
  void setTexcoord0Stride(int stride);
  void texcoord0StrideChanged(int stride) W_SIGNAL(texcoord0StrideChanged, stride);
  
  int texcoord0Buffer() const noexcept { return m_config.texcoord0.buffer_index; }
  void setTexcoord0Buffer(int buf);
  void texcoord0BufferChanged(int buf) W_SIGNAL(texcoord0BufferChanged, buf);

  // Color attribute properties
  int colorLocation() const noexcept { return m_config.color.location; }
  void setColorLocation(int loc);
  void colorLocationChanged(int loc) W_SIGNAL(colorLocationChanged, loc);
  
  int colorFormat() const noexcept { return m_config.color.format; }
  void setColorFormat(int fmt);
  void colorFormatChanged(int fmt) W_SIGNAL(colorFormatChanged, fmt);
  
  int colorOffset() const noexcept { return static_cast<int>(m_config.color.offset); }
  void setColorOffset(int offset);
  void colorOffsetChanged(int offset) W_SIGNAL(colorOffsetChanged, offset);
  
  int colorStride() const noexcept { return static_cast<int>(m_config.color.stride); }
  void setColorStride(int stride);
  void colorStrideChanged(int stride) W_SIGNAL(colorStrideChanged, stride);
  
  int colorBuffer() const noexcept { return m_config.color.buffer_index; }
  void setColorBuffer(int buf);
  void colorBufferChanged(int buf) W_SIGNAL(colorBufferChanged, buf);

  // Tangent attribute properties
  int tangentLocation() const noexcept { return m_config.tangent.location; }
  void setTangentLocation(int loc);
  void tangentLocationChanged(int loc) W_SIGNAL(tangentLocationChanged, loc);
  
  int tangentFormat() const noexcept { return m_config.tangent.format; }
  void setTangentFormat(int fmt);
  void tangentFormatChanged(int fmt) W_SIGNAL(tangentFormatChanged, fmt);
  
  int tangentOffset() const noexcept { return static_cast<int>(m_config.tangent.offset); }
  void setTangentOffset(int offset);
  void tangentOffsetChanged(int offset) W_SIGNAL(tangentOffsetChanged, offset);
  
  int tangentStride() const noexcept { return static_cast<int>(m_config.tangent.stride); }
  void setTangentStride(int stride);
  void tangentStrideChanged(int stride) W_SIGNAL(tangentStrideChanged, stride);
  
  int tangentBuffer() const noexcept { return m_config.tangent.buffer_index; }
  void setTangentBuffer(int buf);
  void tangentBufferChanged(int buf) W_SIGNAL(tangentBufferChanged, buf);

private:
  QString prettyName() const noexcept override;
  
  score::gfx::BufferGeometryNode::Configuration m_config;
};

using ProcessFactory = Process::ProcessFactory_T<Gfx::BufferGeometry::Model>;

}
