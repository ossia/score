#pragma once
#include <Crousti/GpuUtils.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>

#include <ossia/detail/pod_vector.hpp>

#include <boost/container/vector.hpp>

#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <optional>
#include <variant>
namespace Threedim
{
[[nodiscard]] constexpr int32_t attributeFormatSize(halp::attribute_format fmt) noexcept
{
  using namespace halp;
  switch(fmt)
  {
    case attribute_format::float4:
      return 4 * sizeof(float);
    case attribute_format::float3:
      return 3 * sizeof(float);
    case attribute_format::float2:
      return 2 * sizeof(float);
    case attribute_format::float1:
      return sizeof(float);

    case attribute_format::uint4:
      return 4 * sizeof(uint32_t);
    case attribute_format::uint3:
      return 3 * sizeof(uint32_t);
    case attribute_format::uint2:
      return 2 * sizeof(uint32_t);
    case attribute_format::uint1:
      return sizeof(uint32_t);

    case attribute_format::sint4:
      return 4 * sizeof(int32_t);
    case attribute_format::sint3:
      return 3 * sizeof(int32_t);
    case attribute_format::sint2:
      return 2 * sizeof(int32_t);
    case attribute_format::sint1:
      return sizeof(int32_t);

    case attribute_format::unormbyte4:
      return 4 * sizeof(uint8_t);
    case attribute_format::unormbyte2:
      return 2 * sizeof(uint8_t);
    case attribute_format::unormbyte1:
      return sizeof(uint8_t);

    case attribute_format::half4:
      return 4 * sizeof(uint16_t);
    case attribute_format::half3:
      return 3 * sizeof(uint16_t);
    case attribute_format::half2:
      return 2 * sizeof(uint16_t);
    case attribute_format::half1:
      return sizeof(uint16_t);

    case attribute_format::ushort4:
      return 4 * sizeof(uint16_t);
    case attribute_format::ushort3:
      return 3 * sizeof(uint16_t);
    case attribute_format::ushort2:
      return 2 * sizeof(uint16_t);
    case attribute_format::ushort1:
      return sizeof(uint16_t);

    case attribute_format::sshort4:
      return 4 * sizeof(int16_t);
    case attribute_format::sshort3:
      return 3 * sizeof(int16_t);
    case attribute_format::sshort2:
      return 2 * sizeof(int16_t);
    case attribute_format::sshort1:
      return sizeof(int16_t);

    default:
      return 0;
  }
  return 0;
}

[[nodiscard]] constexpr int32_t
attributeFormatComponents(halp::attribute_format fmt) noexcept
{
  using namespace halp;
  switch(fmt)
  {
    case attribute_format::float4:
    case attribute_format::uint4:
    case attribute_format::sint4:
    case attribute_format::unormbyte4:
    case attribute_format::half4:
    case attribute_format::ushort4:
    case attribute_format::sshort4:
      return 4;
    case attribute_format::float3:
    case attribute_format::uint3:
    case attribute_format::sint3:
    case attribute_format::half3:
    case attribute_format::ushort3:
    case attribute_format::sshort3:
      return 3;
    case attribute_format::float2:
    case attribute_format::uint2:
    case attribute_format::sint2:
    case attribute_format::unormbyte2:
    case attribute_format::half2:
    case attribute_format::ushort2:
    case attribute_format::sshort2:
      return 2;
    case attribute_format::float1:
    case attribute_format::uint1:
    case attribute_format::sint1:
    case attribute_format::unormbyte1:
    case attribute_format::half1:
    case attribute_format::ushort1:
    case attribute_format::sshort1:
      return 1;

    default:
      return 0;
  }
  return 0;
}

[[nodiscard]] constexpr bool isFloatFormat(halp::attribute_format fmt) noexcept
{
  const int f = static_cast<int>(fmt);
  return (f >= int(halp::attribute_format::float4)
          && f <= int(halp::attribute_format::float1))
         || (f >= int(halp::attribute_format::half4)
             && f <= int(halp::attribute_format::half1));
}

//=============================================================================
// Attribute Lookup Result
//=============================================================================

struct attribute_lookup
{
  const halp::geometry_attribute* attribute{};
  const halp::geometry_binding* binding{};
  const halp::geometry_input* input{};
  const halp::geometry_gpu_buffer* buffer{};
  int32_t attribute_size{};
  int32_t binding_index{};

  [[nodiscard]] bool valid() const noexcept { return attribute != nullptr; }

  [[nodiscard]] bool canDirectReference() const noexcept
  {
    if(!valid())
      return false;
    return attribute->byte_offset == 0 && attribute_size == binding->stride;
  }
};

[[nodiscard]] inline std::optional<attribute_lookup> findAttribute(
    const halp::dynamic_gpu_geometry& mesh, halp::attribute_location location) noexcept
{
  for(const auto& attr : mesh.attributes)
  {
    if(attr.location == location)
    {
      const auto binding_idx = attr.binding;
      if(binding_idx < 0 || binding_idx >= static_cast<int>(mesh.bindings.size()))
      {
        qDebug() << "GeometryExtraction: Invalid binding index" << binding_idx;
        return std::nullopt;
      }

      if(binding_idx >= static_cast<int>(mesh.input.size()))
      {
        qDebug() << "GeometryExtraction: Missing input for binding" << binding_idx;
        return std::nullopt;
      }

      const auto& inp = mesh.input[binding_idx];
      if(inp.buffer < 0 || inp.buffer >= static_cast<int>(mesh.buffers.size()))
      {
        qDebug() << "GeometryExtraction: Invalid buffer index" << inp.buffer;
        return std::nullopt;
      }

      return attribute_lookup{
          .attribute = &attr,
          .binding = &mesh.bindings[binding_idx],
          .input = &inp,
          .buffer = &mesh.buffers[inp.buffer],
          .attribute_size = attributeFormatSize(attr.format),
          .binding_index = binding_idx};
    }
  }
  return std::nullopt;
}

[[nodiscard]] inline std::optional<attribute_lookup>
findAttribute(const halp::dynamic_gpu_geometry& mesh, int index) noexcept
{
  if(index < 0 || index >= mesh.attributes.size())
    return {};

  const auto& attr = mesh.attributes[index];
  const auto binding_idx = attr.binding;
  if(binding_idx < 0 || binding_idx >= static_cast<int>(mesh.bindings.size()))
  {
    qDebug() << "GeometryExtraction: Invalid binding index" << binding_idx;
    return std::nullopt;
  }

  if(binding_idx >= static_cast<int>(mesh.input.size()))
  {
    qDebug() << "GeometryExtraction: Missing input for binding" << binding_idx;
    return std::nullopt;
  }

  const auto& inp = mesh.input[binding_idx];
  if(inp.buffer < 0 || inp.buffer >= static_cast<int>(mesh.buffers.size()))
  {
    qDebug() << "GeometryExtraction: Invalid buffer index" << inp.buffer;
    return std::nullopt;
  }

  return attribute_lookup{
      .attribute = &attr,
      .binding = &mesh.bindings[binding_idx],
      .input = &inp,
      .buffer = &mesh.buffers[inp.buffer],
      .attribute_size = attributeFormatSize(attr.format),
      .binding_index = binding_idx};
  return std::nullopt;
}
struct gpu_buffer_view
{
  QRhiBuffer* buffer{};
  int64_t offset{};
  int64_t size{};

  [[nodiscard]] bool valid() const noexcept { return buffer != nullptr && size > 0; }
};

class DirectBufferReferenceStrategy
{
public:
  bool init(
      const score::gfx::RenderState& renderState, QRhi& rhi,
      const halp::dynamic_gpu_geometry& mesh, int buffer, int byte_offset, int byte_size)
  {
    m_buffer = static_cast<QRhiBuffer*>(mesh.buffers[buffer].handle);
    m_offset = byte_offset;
    m_size = byte_size;
    assert(m_buffer->size() >= byte_size + byte_offset);

    if(!m_buffer)
    {
      qDebug() << "DirectBufferReferenceStrategy: Null buffer handle";
      return false;
    }
    return true;
  }

  void
  update(QRhi&, const halp::dynamic_gpu_geometry&, const attribute_lookup& lookup, bool)
  {
  }

  void release() noexcept
  {
    m_buffer = nullptr;
    m_offset = 0;
    m_size = 0;
  }

  void runCompute(QRhi&, QRhiCommandBuffer&, QRhiResourceUpdateBatch*&) { }

  [[nodiscard]] gpu_buffer_view output() const noexcept
  {
    return {
        .buffer = m_buffer,
        .offset = m_offset,
        .size = m_size,
    };
  }

  [[nodiscard]] static constexpr bool needsCompute() noexcept { return false; }

private:
  QRhiBuffer* m_buffer{};
  int64_t m_offset{};
  int64_t m_size{};
};

class DirectReferenceStrategy
{
public:
  bool init(
      const score::gfx::RenderState& renderState, QRhi& rhi,
      const halp::dynamic_gpu_geometry& mesh, const attribute_lookup& lookup,
      bool /*padToVec4*/)
  {
    m_buffer = static_cast<QRhiBuffer*>(lookup.buffer->handle);
    m_offset = lookup.input->byte_offset;
    m_size = static_cast<int64_t>(lookup.attribute_size) * mesh.vertices;

    if(!m_buffer)
    {
      qDebug() << "DirectReferenceStrategy: Null buffer handle";
      return false;
    }
    return true;
  }

  void update(
      QRhi& /*rhi*/, const halp::dynamic_gpu_geometry& mesh,
      const attribute_lookup& lookup, bool /*padToVec4*/)
  {
    m_buffer = static_cast<QRhiBuffer*>(lookup.buffer->handle);
    m_offset = lookup.input->byte_offset;
    m_size = static_cast<int64_t>(lookup.attribute_size) * mesh.vertices;
  }

  void release() noexcept
  {
    // We don't own the buffer
    m_buffer = nullptr;
    m_offset = 0;
    m_size = 0;
  }

  void runCompute(QRhi&, QRhiCommandBuffer&, QRhiResourceUpdateBatch*&) { }

  [[nodiscard]] gpu_buffer_view output() const noexcept
  {
    return {
        .buffer = m_buffer,
        .offset = m_offset,
        .size = m_size,
    };
  }

  [[nodiscard]] static constexpr bool needsCompute() noexcept { return false; }

private:
  QRhiBuffer* m_buffer{};
  int64_t m_offset{};
  int64_t m_size{};
};

class ComputeExtractionStrategy
{
public:
  bool init(
      const score::gfx::RenderState& renderState, QRhi& rhi,
      const halp::dynamic_gpu_geometry& mesh, const attribute_lookup& lookup,
      bool padToVec4)
  {
    m_vertexCount = mesh.vertices;
    m_srcStride = lookup.binding->stride;
    m_srcOffset = lookup.attribute->byte_offset
                  + static_cast<int32_t>(lookup.input->byte_offset);
    m_elementCount = attributeFormatComponents(lookup.attribute->format);
    m_padToVec4
        = padToVec4 && m_elementCount < 4 && isFloatFormat(lookup.attribute->format);

    const int32_t outputComponents = m_padToVec4 ? 4 : m_elementCount;
    m_outputComponents = outputComponents;
    m_outputSize
        = static_cast<int64_t>(m_vertexCount) * outputComponents * sizeof(float);

    if(m_outputSize == 0)
    {
      qDebug() << "ComputeExtractionStrategy: Zero output size";
      return false;
    }

    // Create output buffer
    m_outputBuffer = rhi.newBuffer(
        QRhiBuffer::Static, QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer,
        static_cast<quint32>(m_outputSize));

    if(!m_outputBuffer || !m_outputBuffer->create())
    {
      qDebug() << "ComputeExtractionStrategy: Failed to create output buffer";
      return false;
    }

    m_srcBuffer = static_cast<QRhiBuffer*>(lookup.buffer->handle);
    if(!m_srcBuffer)
    {
      qDebug() << "ComputeExtractionStrategy: Null source buffer";
      return false;
    }

    return createPipeline(renderState, rhi);
  }

  void update(
      QRhi& rhi, const halp::dynamic_gpu_geometry& mesh, const attribute_lookup& lookup,
      bool padToVec4)
  {
    m_srcBuffer = static_cast<QRhiBuffer*>(lookup.buffer->handle);
    m_srcStride = lookup.binding->stride;
    m_srcOffset = lookup.attribute->byte_offset
                  + static_cast<int32_t>(lookup.input->byte_offset);

    const bool newPadToVec4
        = padToVec4 && m_elementCount < 4 && isFloatFormat(lookup.attribute->format);
    const int32_t newOutputComponents = newPadToVec4 ? 4 : m_elementCount;
    const int64_t newSize
        = static_cast<int64_t>(mesh.vertices) * newOutputComponents * sizeof(float);

    // Check if buffer needs resize
    if(newSize != m_outputSize || mesh.vertices != m_vertexCount
       || newPadToVec4 != m_padToVec4)
    {
      m_vertexCount = mesh.vertices;
      m_padToVec4 = newPadToVec4;
      m_outputComponents = newOutputComponents;
      m_outputSize = newSize;

      if(m_outputSize > 0)
      {
        m_outputBuffer->setSize(static_cast<quint32>(m_outputSize));
        m_outputBuffer->create();
      }
    }

    // Rebind if source buffer changed
    if(m_srb)
    {
      m_srb->setBindings({
          QRhiShaderResourceBinding::bufferLoad(
              0, QRhiShaderResourceBinding::ComputeStage, m_srcBuffer),
          QRhiShaderResourceBinding::bufferStore(
              1, QRhiShaderResourceBinding::ComputeStage, m_outputBuffer),
      });
      m_srb->create();
    }

    m_dirty = true;
  }

  void release() noexcept
  {
    delete m_uniformBuffer;
    m_uniformBuffer = nullptr;

    delete m_pipeline;
    m_pipeline = nullptr;

    delete m_srb;
    m_srb = nullptr;

    delete m_outputBuffer;
    m_outputBuffer = nullptr;

    m_srcBuffer = nullptr;
  }

  void runCompute(QRhi& rhi, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& res)
  {
    if(!m_dirty || m_vertexCount == 0 || !m_pipeline)
      return;

    struct alignas(16) Params
    {
      uint32_t vertexCount;
      uint32_t srcStrideBytes;
      uint32_t srcOffsetBytes;
      uint32_t elementCount;
      uint32_t padToVec4;
      uint32_t _pad[3]; // Padding to maintain alignment
    } params{
        static_cast<uint32_t>(m_vertexCount),
        static_cast<uint32_t>(m_srcStride),
        static_cast<uint32_t>(m_srcOffset),
        static_cast<uint32_t>(m_elementCount),
        m_padToVec4 ? 1u : 0u,
        {0, 0, 0}};

    res->updateDynamicBuffer(m_uniformBuffer, 0, sizeof(params), &params);

    cb.beginComputePass(res);
    cb.setComputePipeline(m_pipeline);
    cb.setShaderResources(m_srb);

    const int workgroups = (m_vertexCount + 255) / 256;
    cb.dispatch(workgroups, 1, 1);

    cb.endComputePass();

    // Get new resource batch after compute pass
    res = rhi.nextResourceUpdateBatch();

    m_dirty = false;
  }

  [[nodiscard]] gpu_buffer_view output() const noexcept
  {
    return {
        .buffer = m_outputBuffer,
        .offset = 0,
        .size = m_outputSize,
    };
  }

  [[nodiscard]] static constexpr bool needsCompute() noexcept { return true; }

private:
  bool createPipeline(const score::gfx::RenderState& renderState, QRhi& rhi)
  {
    static const QString shaderCode = QStringLiteral(R"(#version 450

layout(local_size_x = 256) in;

layout(std140, binding = 0) uniform Params {
    uint vertexCount;
    uint srcStrideBytes;
    uint srcOffsetBytes;
    uint elementCount;
    uint padToVec4;
};

layout(std430, binding = 1) readonly buffer SrcBuffer {
    uint src_data[];
};

layout(std430, binding = 2) writeonly buffer DstBuffer {
    uint dst_data[];
};

void main()
{
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= vertexCount)
        return;

    uint srcBase = (idx * srcStrideBytes + srcOffsetBytes) / 4;
    uint dstComponents = padToVec4 != 0 ? 4 : elementCount;
    uint dstBase = idx * dstComponents;

    for (uint i = 0; i < elementCount; ++i)
        dst_data[dstBase + i] = src_data[srcBase + i];

    if (padToVec4 != 0)
    {
        for (uint i = elementCount; i < 4; ++i)
            dst_data[dstBase + i] = (i == 3) ? 0x3f800000u : 0u;
    }
}
)");

    QShader shader = score::gfx::makeCompute(renderState, shaderCode);
    if(!shader.isValid())
    {
      qDebug() << "ComputeExtractionStrategy: Shader compilation failed";
      return false;
    }

    // Create uniform buffer (aligned to 256 bytes for compatibility)
    m_uniformBuffer = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 256);

    if(!m_uniformBuffer || !m_uniformBuffer->create())
    {
      qDebug() << "ComputeExtractionStrategy: UBO creation failed";
      return false;
    }

    m_srb = rhi.newShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::ComputeStage, m_uniformBuffer),
        QRhiShaderResourceBinding::bufferLoad(
            1, QRhiShaderResourceBinding::ComputeStage, m_srcBuffer),
        QRhiShaderResourceBinding::bufferStore(
            2, QRhiShaderResourceBinding::ComputeStage, m_outputBuffer),
    });

    if(!m_srb->create())
    {
      qDebug() << "ComputeExtractionStrategy: SRB creation failed";
      return false;
    }

    m_pipeline = rhi.newComputePipeline();
    m_pipeline->setShaderResourceBindings(m_srb);
    m_pipeline->setShaderStage({QRhiShaderStage::Compute, shader});

    if(!m_pipeline->create())
    {
      qDebug() << "ComputeExtractionStrategy: Pipeline creation failed";
      return false;
    }

    m_dirty = true;
    return true;
  }

  QRhiBuffer* m_srcBuffer{};
  QRhiBuffer* m_uniformBuffer{};
  QRhiBuffer* m_outputBuffer{};
  QRhiShaderResourceBindings* m_srb{};
  QRhiComputePipeline* m_pipeline{};

  int32_t m_vertexCount{};
  int32_t m_srcStride{};
  int32_t m_srcOffset{};
  int32_t m_elementCount{};
  int32_t m_outputComponents{};
  int64_t m_outputSize{};

  bool m_padToVec4{false};
  bool m_dirty{true};
};

class IndexedExtractionStrategy
{
public:
  bool init(
      const score::gfx::RenderState& renderState, QRhi& rhi,
      const halp::dynamic_gpu_geometry& mesh, const attribute_lookup& lookup,
      bool padToVec4)
  {
    if(mesh.index.buffer < 0
       || mesh.index.buffer >= static_cast<int>(mesh.buffers.size()))
    {
      qDebug() << "IndexedExtractionStrategy: Invalid index buffer";
      return false;
    }

    // For indexed geometry, we output one vertex per index
    m_indexCount = mesh.vertices;
    m_srcStride = lookup.binding->stride;
    m_srcOffset = lookup.attribute->byte_offset
                  + static_cast<int32_t>(lookup.input->byte_offset);
    m_elementCount = attributeFormatComponents(lookup.attribute->format);
    m_padToVec4
        = padToVec4 && m_elementCount < 4 && isFloatFormat(lookup.attribute->format);
    m_indexFormat32 = (mesh.index.format == halp::index_format::uint32);
    m_indexOffset = static_cast<int32_t>(mesh.index.byte_offset);

    const int32_t outputComponents = m_padToVec4 ? 4 : m_elementCount;
    m_outputComponents = outputComponents;
    m_outputSize = static_cast<int64_t>(m_indexCount) * outputComponents * sizeof(float);

    if(m_outputSize == 0)
    {
      qDebug() << "IndexedExtractionStrategy: Zero output size";
      return false;
    }

    m_outputBuffer = rhi.newBuffer(
        QRhiBuffer::Static, QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer,
        static_cast<quint32>(m_outputSize));

    if(!m_outputBuffer || !m_outputBuffer->create())
    {
      qDebug() << "IndexedExtractionStrategy: Failed to create output buffer";
      return false;
    }

    m_srcBuffer = static_cast<QRhiBuffer*>(lookup.buffer->handle);
    m_indexBuffer = static_cast<QRhiBuffer*>(mesh.buffers[mesh.index.buffer].handle);

    if(!m_srcBuffer || !m_indexBuffer)
    {
      qDebug() << "IndexedExtractionStrategy: Null source or index buffer";
      return false;
    }

    return createPipeline(renderState, rhi);
  }

  void update(
      QRhi& rhi, const halp::dynamic_gpu_geometry& mesh, const attribute_lookup& lookup,
      bool padToVec4)
  {
    m_srcBuffer = static_cast<QRhiBuffer*>(lookup.buffer->handle);
    m_srcStride = lookup.binding->stride;
    m_srcOffset = lookup.attribute->byte_offset
                  + static_cast<int32_t>(lookup.input->byte_offset);

    if(mesh.index.buffer >= 0
       && mesh.index.buffer < static_cast<int>(mesh.buffers.size()))
    {
      m_indexBuffer = static_cast<QRhiBuffer*>(mesh.buffers[mesh.index.buffer].handle);
      m_indexOffset = static_cast<int32_t>(mesh.index.byte_offset);
      m_indexFormat32 = (mesh.index.format == halp::index_format::uint32);
    }

    const bool newPadToVec4
        = padToVec4 && m_elementCount < 4 && isFloatFormat(lookup.attribute->format);
    const int32_t newOutputComponents = newPadToVec4 ? 4 : m_elementCount;
    const int64_t newSize
        = static_cast<int64_t>(mesh.vertices) * newOutputComponents * sizeof(float);

    if(newSize != m_outputSize || mesh.vertices != m_indexCount
       || newPadToVec4 != m_padToVec4)
    {
      m_indexCount = mesh.vertices;
      m_padToVec4 = newPadToVec4;
      m_outputComponents = newOutputComponents;
      m_outputSize = newSize;

      if(m_outputSize > 0)
      {
        m_outputBuffer->setSize(static_cast<quint32>(m_outputSize));
        m_outputBuffer->create();
      }
    }

    if(m_srb)
    {
      m_srb->setBindings({
          QRhiShaderResourceBinding::bufferLoad(
              0, QRhiShaderResourceBinding::ComputeStage, m_srcBuffer),
          QRhiShaderResourceBinding::bufferLoad(
              1, QRhiShaderResourceBinding::ComputeStage, m_indexBuffer),
          QRhiShaderResourceBinding::bufferStore(
              2, QRhiShaderResourceBinding::ComputeStage, m_outputBuffer),
      });
      m_srb->create();
    }

    m_dirty = true;
  }

  void release() noexcept
  {
    delete m_pipeline;
    m_pipeline = nullptr;

    delete m_srb;
    m_srb = nullptr;

    delete m_uniformBuffer;
    m_uniformBuffer = nullptr;

    delete m_outputBuffer;
    m_outputBuffer = nullptr;

    m_srcBuffer = nullptr;
    m_indexBuffer = nullptr;
  }

  void runCompute(QRhi& rhi, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& res)
  {
    if(!m_dirty || m_indexCount == 0 || !m_pipeline)
      return;

    struct alignas(16) Params
    {
      uint32_t indexCount;
      uint32_t srcStrideBytes;
      uint32_t srcOffsetBytes;
      uint32_t elementCount;
      uint32_t padToVec4;
      uint32_t indexOffsetBytes;
      uint32_t index32Bit;
      uint32_t _pad;
    } params{
        static_cast<uint32_t>(m_indexCount),
        static_cast<uint32_t>(m_srcStride),
        static_cast<uint32_t>(m_srcOffset),
        static_cast<uint32_t>(m_elementCount),
        m_padToVec4 ? 1u : 0u,
        static_cast<uint32_t>(m_indexOffset),
        m_indexFormat32 ? 1u : 0u,
        0};

    res->updateDynamicBuffer(m_uniformBuffer, 0, sizeof(params), &params);

    cb.beginComputePass(res);
    cb.setComputePipeline(m_pipeline);
    cb.setShaderResources(m_srb);

    const int workgroups = (m_indexCount + 255) / 256;
    cb.dispatch(workgroups, 1, 1);

    cb.endComputePass();

    res = rhi.nextResourceUpdateBatch();

    m_dirty = false;
  }

  [[nodiscard]] gpu_buffer_view output() const noexcept
  {
    return {
        .buffer = m_outputBuffer,
        .offset = 0,
        .size = m_outputSize,
    };
  }

  [[nodiscard]] static constexpr bool needsCompute() noexcept { return true; }

private:
  bool createPipeline(const score::gfx::RenderState& renderState, QRhi& rhi)
  {
    static const QString shaderCode = QStringLiteral(R"(#version 450

layout(local_size_x = 256) in;

layout(std140, binding = 0) uniform Params {
    uint indexCount;
    uint srcStrideBytes;
    uint srcOffsetBytes;
    uint elementCount;
    uint padToVec4;
    uint indexOffsetBytes;
    uint index32Bit;
};

layout(std430, binding = 1) readonly buffer SrcBuffer {
    uint src_data[];
};

layout(std430, binding = 2) readonly buffer IndexBuffer {
    uint index_data[];
};

layout(std430, binding = 3) writeonly buffer DstBuffer {
    uint dst_data[];
};

uint readIndex(uint i)
{
    if (index32Bit != 0)
    {
        uint wordIndex = (indexOffsetBytes / 4) + i;
        return index_data[wordIndex];
    }
    else
    {
        uint bytePos = indexOffsetBytes + i * 2;
        uint wordIndex = bytePos / 4;
        uint word = index_data[wordIndex];
        uint shift = (bytePos % 4) * 8;
        return (word >> shift) & 0xFFFFu;
    }
}

void main()
{
    uint outputIdx = gl_GlobalInvocationID.x;
    if (outputIdx >= indexCount)
        return;

    uint vertexIdx = readIndex(outputIdx);
    uint srcBase = (vertexIdx * srcStrideBytes + srcOffsetBytes) / 4;
    uint dstComponents = padToVec4 != 0 ? 4 : elementCount;
    uint dstBase = outputIdx * dstComponents;

    for (uint i = 0; i < elementCount; ++i)
        dst_data[dstBase + i] = src_data[srcBase + i];

    if (padToVec4 != 0)
    {
        for (uint i = elementCount; i < 4; ++i)
            dst_data[dstBase + i] = (i == 3) ? 0x3f800000u : 0u;
    }
}
)");

    QShader shader = score::gfx::makeCompute(renderState, shaderCode);
    if(!shader.isValid())
    {
      qDebug() << "IndexedExtractionStrategy: Shader compilation failed";
      return false;
    }

    m_uniformBuffer = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 256);

    if(!m_uniformBuffer || !m_uniformBuffer->create())
    {
      qDebug() << "IndexedExtractionStrategy: UBO creation failed";
      return false;
    }

    m_srb = rhi.newShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::ComputeStage, m_uniformBuffer),
        QRhiShaderResourceBinding::bufferLoad(
            1, QRhiShaderResourceBinding::ComputeStage, m_srcBuffer),
        QRhiShaderResourceBinding::bufferLoad(
            2, QRhiShaderResourceBinding::ComputeStage, m_indexBuffer),
        QRhiShaderResourceBinding::bufferStore(
            3, QRhiShaderResourceBinding::ComputeStage, m_outputBuffer),
    });

    if(!m_srb->create())
    {
      qDebug() << "IndexedExtractionStrategy: SRB creation failed";
      return false;
    }

    m_pipeline = rhi.newComputePipeline();
    m_pipeline->setShaderResourceBindings(m_srb);
    m_pipeline->setShaderStage({QRhiShaderStage::Compute, shader});

    if(!m_pipeline->create())
    {
      qDebug() << "IndexedExtractionStrategy: Pipeline creation failed";
      return false;
    }

    m_dirty = true;
    return true;
  }

  QRhiBuffer* m_srcBuffer{};
  QRhiBuffer* m_uniformBuffer{};
  QRhiBuffer* m_indexBuffer{};
  QRhiBuffer* m_outputBuffer{};
  QRhiShaderResourceBindings* m_srb{};
  QRhiComputePipeline* m_pipeline{};

  int32_t m_indexCount{};
  int32_t m_srcStride{};
  int32_t m_srcOffset{};
  int32_t m_indexOffset{};
  int32_t m_elementCount{};
  int32_t m_outputComponents{};
  int64_t m_outputSize{};

  bool m_padToVec4{false};
  bool m_indexFormat32{true};
  bool m_dirty{true};
};

using ExtractionStrategyVariant = std::variant<
    std::monostate, DirectBufferReferenceStrategy, DirectReferenceStrategy,
    ComputeExtractionStrategy, IndexedExtractionStrategy>;

}
