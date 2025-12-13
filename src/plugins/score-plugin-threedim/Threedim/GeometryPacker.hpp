#pragma once
#include <Threedim/GeometryToBufferStrategies.hpp>
#include <halp/buffer.hpp>
#include <halp/meta.hpp>

#include <array>

namespace score::gfx
{
class RenderList;
struct Edge;
}

namespace Threedim
{
struct packed_attribute_spec
{
  halp::attribute_location location{};
  bool pad_to_vec4{false};
};

struct packed_attribute_info
{
  int32_t src_buffer_index{-1}; // Index into mesh.buffers
  int32_t src_stride{};
  int32_t src_offset{}; // attribute offset + input offset
  int32_t element_count{};
  int32_t output_components{}; // element_count or 4 if padded
  int32_t output_offset{};     // Offset in output stride
  bool is_float{true};
  bool is_active{false};
};

class PackedExtractionStrategy
{
  static constexpr int MAX_PACKED_ATTRIBUTES = 8;

public:
  bool init(
      const score::gfx::RenderState& renderState, QRhi& rhi,
      const halp::dynamic_gpu_geometry& mesh,
      std::span<const packed_attribute_spec> specs)
  {
    if(specs.empty() || specs.size() > MAX_PACKED_ATTRIBUTES)
    {
      qDebug() << "PackedExtractionStrategy: Invalid attribute count:" << specs.size();
      return false;
    }

    m_vertexCount = mesh.vertices;
    m_hasIndexBuffer = mesh.index.buffer >= 0;
    m_attributeCount = 0;
    m_outputStride = 0;

    // Gather unique source buffers
    m_srcBufferCount = 0;
    std::fill(std::begin(m_srcBufferMapping), std::end(m_srcBufferMapping), -1);

    // Process each requested attribute
    for(const auto& spec : specs)
    {
      auto lookup = findAttribute(mesh, spec.location);
      if(!lookup)
      {
        qDebug() << "PackedExtractionStrategy: Attribute not found:"
                 << magic_enum::enum_name(spec.location);
        continue;
      }

      auto& info = m_attributes[m_attributeCount];
      info.is_active = true;
      info.src_buffer_index = lookup->input->buffer;
      info.src_stride = lookup->binding->stride;
      info.src_offset = lookup->attribute->byte_offset
                        + static_cast<int32_t>(lookup->input->byte_offset);
      info.element_count = attributeFormatComponents(lookup->attribute->format);
      info.is_float = isFloatFormat(lookup->attribute->format);
      info.output_components
          = (spec.pad_to_vec4 && info.element_count < 4 && info.is_float)
                ? 4
                : info.element_count;
      info.output_offset = m_outputStride;

      m_outputStride += info.output_components * sizeof(float);

      // Track unique source buffers
      int mappedIndex = m_srcBufferMapping[info.src_buffer_index];
      if(mappedIndex < 0)
      {
        if(m_srcBufferCount >= MAX_PACKED_ATTRIBUTES)
        {
          qDebug() << "PackedExtractionStrategy: Too many source buffers";
          return false;
        }
        m_srcBufferMapping[info.src_buffer_index] = m_srcBufferCount;
        m_srcBuffers[m_srcBufferCount]
            = static_cast<QRhiBuffer*>(mesh.buffers[info.src_buffer_index].handle);
        ++m_srcBufferCount;
      }

      ++m_attributeCount;
    }

    if(m_attributeCount == 0)
    {
      qDebug() << "PackedExtractionStrategy: No valid attributes found";
      return false;
    }

    // Handle index buffer
    if(m_hasIndexBuffer)
    {
      if(mesh.index.buffer < 0
         || mesh.index.buffer >= static_cast<int>(mesh.buffers.size()))
      {
        qDebug() << "PackedExtractionStrategy: Invalid index buffer";
        return false;
      }
      m_indexBuffer = static_cast<QRhiBuffer*>(mesh.buffers[mesh.index.buffer].handle);
      m_indexOffset = static_cast<int32_t>(mesh.index.byte_offset);
      m_indexFormat32 = (mesh.index.format == halp::index_format::uint32);
    }

    m_outputSize = static_cast<int64_t>(m_vertexCount) * m_outputStride;

    if(m_outputSize == 0)
    {
      qDebug() << "PackedExtractionStrategy: Zero output size";
      return false;
    }

    // Create output buffer
    m_outputBuffer = rhi.newBuffer(
        QRhiBuffer::Static, QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer,
        static_cast<quint32>(m_outputSize));
    m_outputBuffer->setName("GeometryPacker::m_outputBuffer");

    if(!m_outputBuffer || !m_outputBuffer->create())
    {
      qDebug() << "PackedExtractionStrategy: Failed to create output buffer";
      return false;
    }

    return createPipeline(renderState, rhi);
  }

  void update(
      QRhi& rhi, const halp::dynamic_gpu_geometry& mesh,
      std::span<const packed_attribute_spec> specs)
  {
    // Check if vertex count changed
    const int64_t newSize = static_cast<int64_t>(mesh.vertices) * m_outputStride;

    if(newSize != m_outputSize || mesh.vertices != m_vertexCount)
    {
      m_vertexCount = mesh.vertices;
      m_outputSize = newSize;

      if(m_outputSize > 0)
      {
        m_outputBuffer->setSize(static_cast<quint32>(m_outputSize));
        m_outputBuffer->create();
      }
    }

    // Update source buffer handles (they may have been recreated)
    std::fill(std::begin(m_srcBufferMapping), std::end(m_srcBufferMapping), -1);
    m_srcBufferCount = 0;

    int attrIdx = 0;
    for(const auto& spec : specs)
    {
      if(attrIdx >= m_attributeCount)
        break;

      auto lookup = findAttribute(mesh, spec.location);
      if(!lookup)
        continue;

      auto& info = m_attributes[attrIdx];
      info.src_buffer_index = lookup->input->buffer;
      info.src_stride = lookup->binding->stride;
      info.src_offset = lookup->attribute->byte_offset
                        + static_cast<int32_t>(lookup->input->byte_offset);

      int mappedIndex = m_srcBufferMapping[info.src_buffer_index];
      if(mappedIndex < 0)
      {
        m_srcBufferMapping[info.src_buffer_index] = m_srcBufferCount;
        m_srcBuffers[m_srcBufferCount]
            = static_cast<QRhiBuffer*>(mesh.buffers[info.src_buffer_index].handle);
        ++m_srcBufferCount;
      }

      ++attrIdx;
    }

    // Update index buffer
    if(m_hasIndexBuffer && mesh.index.buffer >= 0)
    {
      m_indexBuffer = static_cast<QRhiBuffer*>(mesh.buffers[mesh.index.buffer].handle);
      m_indexOffset = static_cast<int32_t>(mesh.index.byte_offset);
      m_indexFormat32 = (mesh.index.format == halp::index_format::uint32);
    }

    updateBindings();
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

    // Freed by the engine:
    // delete m_outputBuffer;
    // m_outputBuffer = nullptr;

    std::fill(std::begin(m_srcBuffers), std::end(m_srcBuffers), nullptr);
    m_indexBuffer = nullptr;
  }

  void runCompute(QRhi& rhi, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& res)
  {
    if(!m_dirty || m_vertexCount == 0 || !m_pipeline)
      return;

    // Prepare UBO data
    struct alignas(16) AttributeParams
    {
      uint32_t srcBufferIndex; // Which source buffer (0-7)
      uint32_t srcStrideBytes;
      uint32_t srcOffsetBytes;
      uint32_t elementCount;
      uint32_t outputComponents;
      uint32_t outputOffsetBytes;
      uint32_t isActive;
      uint32_t _pad;
    };

    struct alignas(64) Params
    {
      uint32_t vertexCount;
      uint32_t attributeCount;
      uint32_t outputStrideBytes;
      uint32_t hasIndexBuffer;
      uint32_t indexOffsetBytes;
      uint32_t index32Bit;
      uint32_t _pad[2];
      alignas(64) AttributeParams attributes[MAX_PACKED_ATTRIBUTES];
    } params{};
    static_assert(offsetof(Params, attributes[0]) == 64);

    params.vertexCount = static_cast<uint32_t>(m_vertexCount);
    params.attributeCount = static_cast<uint32_t>(m_attributeCount);
    params.outputStrideBytes = static_cast<uint32_t>(m_outputStride);
    params.hasIndexBuffer = m_hasIndexBuffer ? 1u : 0u;
    params.indexOffsetBytes = static_cast<uint32_t>(m_indexOffset);
    params.index32Bit = m_indexFormat32 ? 1u : 0u;

    for(int i = 0; i < MAX_PACKED_ATTRIBUTES; ++i)
    {
      const auto& info = m_attributes[i];
      auto& ap = params.attributes[i];

      if(info.is_active)
      {
        ap.srcBufferIndex
            = static_cast<uint32_t>(m_srcBufferMapping[info.src_buffer_index]);
        ap.srcStrideBytes = static_cast<uint32_t>(info.src_stride);
        ap.srcOffsetBytes = static_cast<uint32_t>(info.src_offset);
        ap.elementCount = static_cast<uint32_t>(info.element_count);
        ap.outputComponents = static_cast<uint32_t>(info.output_components);
        ap.outputOffsetBytes = static_cast<uint32_t>(info.output_offset);
        ap.isActive = 1u;
      }
      else
      {
        ap.isActive = 0u;
      }
    }

    res->updateDynamicBuffer(m_uniformBuffer, 0, sizeof(params), &params);

    cb.beginComputePass(res);
    cb.setComputePipeline(m_pipeline);
    cb.setShaderResources(m_srb);

    const int workgroups = (m_vertexCount + 255) / 256;
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

  [[nodiscard]] int32_t outputStride() const noexcept { return m_outputStride; }

  [[nodiscard]] int32_t attributeCount() const noexcept { return m_attributeCount; }

  [[nodiscard]] const packed_attribute_info& attributeInfo(int index) const noexcept
  {
    return m_attributes[index];
  }

  [[nodiscard]] static constexpr bool needsCompute() noexcept { return true; }

private:
  bool createPipeline(const score::gfx::RenderState& renderState, QRhi& rhi)
  {
    m_uniformBuffer = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 512);
    m_uniformBuffer->setName("GeometryPacker::m_uniformBuffer");

    if(!m_uniformBuffer || !m_uniformBuffer->create())
    {
      qDebug() << "PackedExtractionStrategy: UBO creation failed";
      return false;
    }

    static const QString shaderCode = QStringLiteral(R"(#version 450

layout(local_size_x = 256) in;

struct AttributeParams {
    uint srcBufferIndex;
    uint srcStrideBytes;
    uint srcOffsetBytes;
    uint elementCount;
    uint outputComponents;
    uint outputOffsetBytes;
    uint isActive;
    uint _pad;
};

layout(std140, binding = 0) uniform Params {
    uint vertexCount;
    uint attributeCount;
    uint outputStrideBytes;
    uint hasIndexBuffer;
    uint indexOffsetBytes;
    uint index32Bit;
    uint _pad[2];
    AttributeParams attributes[8];
};

layout(std430, binding = 1) readonly buffer SrcBuffer0 { uint src0[]; };
layout(std430, binding = 2) readonly buffer SrcBuffer1 { uint src1[]; };
layout(std430, binding = 3) readonly buffer SrcBuffer2 { uint src2[]; };
layout(std430, binding = 4) readonly buffer SrcBuffer3 { uint src3[]; };
layout(std430, binding = 5) readonly buffer SrcBuffer4 { uint src4[]; };
layout(std430, binding = 6) readonly buffer SrcBuffer5 { uint src5[]; };
layout(std430, binding = 7) readonly buffer SrcBuffer6 { uint src6[]; };
layout(std430, binding = 8) readonly buffer SrcBuffer7 { uint src7[]; };

layout(std430, binding = 9) readonly buffer IndexBuffer { uint index_data[]; };

layout(std430, binding = 10) writeonly buffer DstBuffer { uint dst_data[]; };

uint readSrcData(uint bufferIndex, uint wordIndex)
{
    switch (bufferIndex)
    {
        case 0: return src0[wordIndex];
        case 1: return src1[wordIndex];
        case 2: return src2[wordIndex];
        case 3: return src3[wordIndex];
        case 4: return src4[wordIndex];
        case 5: return src5[wordIndex];
        case 6: return src6[wordIndex];
        case 7: return src7[wordIndex];
    }
    return 0;
}

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
    if (outputIdx >= vertexCount)
        return;

    // Get source vertex index (may differ from output index if indexed)
    uint srcVertexIdx = hasIndexBuffer != 0 ? readIndex(outputIdx) : outputIdx;

    // Output base position (in uints, since outputStrideBytes is in bytes)
    uint dstBase = (outputIdx * outputStrideBytes) / 4;

    // Process each attribute
    for (uint a = 0; a < attributeCount; ++a)
    {
        AttributeParams attr = attributes[a];
        if (attr.isActive == 0)
            continue;

        // Source position
        uint srcBase = (srcVertexIdx * attr.srcStrideBytes + attr.srcOffsetBytes) / 4;
        
        // Destination position for this attribute
        uint attrDstBase = dstBase + (attr.outputOffsetBytes / 4);

        // Copy elements
        for (uint i = 0; i < attr.elementCount; ++i)
        {
            uint value = readSrcData(attr.srcBufferIndex, srcBase + i);
            dst_data[attrDstBase + i] = value;
        }

        // Pad to vec4 if needed
        if (attr.outputComponents > attr.elementCount)
        {
            for (uint i = attr.elementCount; i < attr.outputComponents; ++i)
            {
                // w = 1.0f (0x3f800000), others = 0
                dst_data[attrDstBase + i] = (i == 3) ? 0x3f800000u : 0u;
            }
        }
    }
}
)");

    QShader shader = score::gfx::makeCompute(renderState, shaderCode);
    if(!shader.isValid())
    {
      qDebug() << "PackedExtractionStrategy: Shader compilation failed";
      return false;
    }

    m_srb = rhi.newShaderResourceBindings();
    updateBindings();

    if(!m_srb->create())
    {
      qDebug() << "PackedExtractionStrategy: SRB creation failed";
      return false;
    }

    m_pipeline = rhi.newComputePipeline();
    m_pipeline->setShaderResourceBindings(m_srb);
    m_pipeline->setShaderStage({QRhiShaderStage::Compute, shader});

    if(!m_pipeline->create())
    {
      qDebug() << "PackedExtractionStrategy: Pipeline creation failed";
      return false;
    }

    m_dirty = true;
    return true;
  }

  void updateBindings()
  {
    if(!m_srb)
      return;

    QVarLengthArray<QRhiShaderResourceBinding, 12> bindings;

    // Binding 0: UBO
    bindings.append(
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::ComputeStage, m_uniformBuffer));

    // Bindings 1-8: Source buffers (use first valid buffer as placeholder for unused slots)
    QRhiBuffer* placeholderBuffer = nullptr;
    for(int i = 0; i < m_srcBufferCount; ++i)
    {
      if(m_srcBuffers[i])
      {
        placeholderBuffer = m_srcBuffers[i];
        break;
      }
    }
    if(!placeholderBuffer && m_outputBuffer)
    {
      placeholderBuffer = m_outputBuffer; // Fallback
    }

    for(int i = 0; i < MAX_PACKED_ATTRIBUTES; ++i)
    {
      QRhiBuffer* buf = (i < m_srcBufferCount && m_srcBuffers[i]) ? m_srcBuffers[i]
                                                                  : placeholderBuffer;
      bindings.append(
          QRhiShaderResourceBinding::bufferLoad(
              1 + i, QRhiShaderResourceBinding::ComputeStage, buf));
    }

    // Binding 9: Index buffer (use placeholder if not indexed)
    QRhiBuffer* idxBuf
        = m_hasIndexBuffer && m_indexBuffer ? m_indexBuffer : placeholderBuffer;
    bindings.append(
        QRhiShaderResourceBinding::bufferLoad(
            9, QRhiShaderResourceBinding::ComputeStage, idxBuf));

    // Binding 10: Output buffer
    bindings.append(
        QRhiShaderResourceBinding::bufferStore(
            10, QRhiShaderResourceBinding::ComputeStage, m_outputBuffer));

    m_srb->setBindings(bindings.cbegin(), bindings.cend());
  }

  // Source buffers (up to MAX_PACKED_ATTRIBUTES unique buffers)
  QRhiBuffer* m_srcBuffers[MAX_PACKED_ATTRIBUTES]{};
  int m_srcBufferMapping[32]{}; // mesh buffer index -> m_srcBuffers index
  int m_srcBufferCount{};

  QRhiBuffer* m_indexBuffer{};
  QRhiBuffer* m_outputBuffer{};
  QRhiShaderResourceBindings* m_srb{};
  QRhiComputePipeline* m_pipeline{};
  QRhiBuffer* m_uniformBuffer{};

  packed_attribute_info m_attributes[MAX_PACKED_ATTRIBUTES]{};
  int32_t m_attributeCount{};
  int32_t m_vertexCount{};
  int32_t m_outputStride{}; // In bytes
  int32_t m_indexOffset{};
  int64_t m_outputSize{};

  bool m_hasIndexBuffer{false};
  bool m_indexFormat32{true};
  bool m_dirty{true};
};

class GeometryPacker
{
public:
  halp_meta(name, "Repack attributes")
  halp_meta(category, "Visuals/3D")
  halp_meta(c_name, "pack_geometry")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/pack-geometry.html")
  halp_meta(uuid, "7d7d5973-4aa9-4bfe-9249-8b892d92e0db")

  enum class Attribute3
  {
    None,
    Vec3,
    Vec4
  };
  enum class Attribute2
  {
    None,
    Vec2
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

    halp::combobox_t<"Position", Attribute3> position;
    halp::combobox_t<"Normal", Attribute3> normal;
    halp::combobox_t<"Color", Attribute3> color;
    halp::combobox_t<"TexCoord", Attribute2> texcoord;
    halp::combobox_t<"Tangent", Attribute3> tangent;
  } inputs;

  struct outs
  {
    halp::gpu_buffer_output<"Buffer"> buffer;

    struct
    {
      halp_meta(name, "Stride");
      int32_t value{};
    } stride;
  } outputs;

  GeometryPacker() = default;
  ~GeometryPacker() = default;

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res);

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e);

  void release(score::gfx::RenderList& r);

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge);

  void operator()() { }

private:
  void buildAttributeSpecs();
  bool specsChanged() const;

  PackedExtractionStrategy m_strategy;
  std::vector<packed_attribute_spec> m_specs;
  std::vector<packed_attribute_spec> m_lastSpecs;
  bool m_initialized{false};
};

}
