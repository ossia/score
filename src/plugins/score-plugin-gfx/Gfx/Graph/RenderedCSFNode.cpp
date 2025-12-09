#include "ossia/detail/fmt.hpp"

#include <Gfx/Graph/CommonUBOs.hpp>
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/RenderedCSFNode.hpp>
#include <Gfx/Graph/RenderedISFSamplerUtils.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/algorithms.hpp>

#include <boost/algorithm/string/replace.hpp>

namespace score::gfx
{
static QRhiTexture::Format
getTextureFormat(const QString& format)  noexcept
{
  // Map CSF format strings to Qt RHI texture formats
  if(format == "RGBA8")
    return QRhiTexture::RGBA8;
  else if(format == "BGRA8")
    return QRhiTexture::BGRA8;
  else if(format == "R8")
    return QRhiTexture::R8;

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  else if(format == "RG8")
    return QRhiTexture::RG8;
#endif
  else if(format == "R16")
    return QRhiTexture::R16;

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  else if(format == "RG16")
    return QRhiTexture::RG16;
#endif
  else if(format == "RGBA16F") return QRhiTexture::RGBA16F;
  else if(format == "RGBA32F") return QRhiTexture::RGBA32F;
  else if(format == "R16F")
    return QRhiTexture::R16F;
  else if(format == "R32F")
    return QRhiTexture::R32F;

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  else if(format == "RGB10A2")
    return QRhiTexture::RGB10A2;
#endif
  // Default to RGBA8 if format not recognized
  return QRhiTexture::RGBA8;
}

static QString rhiTextureFormatToShaderLayoutFormatString(QRhiTexture::Format fmt)
{
  switch(fmt)
  {
    case QRhiTexture::Format::RGBA8:
    case QRhiTexture::Format::BGRA8:
      return QStringLiteral("rgba8");
    case QRhiTexture::Format::RGBA16F:
      return QStringLiteral("rgba16f");
    case QRhiTexture::Format::RGBA32F:
      return QStringLiteral("rgba32f");

    case QRhiTexture::Format::R8:
    case QRhiTexture::Format::RED_OR_ALPHA8:
      return QStringLiteral("r8");

    case QRhiTexture::Format::R16:
      return QStringLiteral("r16");
    case QRhiTexture::Format::R16F:
      return QStringLiteral("r16f");
    case QRhiTexture::Format::R32F:
      return QStringLiteral("r32f");

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    case QRhiTexture::RG8:
      return QStringLiteral("rg8");
    case QRhiTexture::RG16:
      return QStringLiteral("rg16");
    case QRhiTexture::RGB10A2:
      return QStringLiteral("rgba10_a2");
    case QRhiTexture::Format::D24:
      return QStringLiteral("r32ui");
    case QRhiTexture::Format::D24S8:
      return QStringLiteral("r32ui");
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    case QRhiTexture::Format::R8UI:
      return QStringLiteral("r8ui");
    case QRhiTexture::Format::R32UI:
      return QStringLiteral("r32ui");
    case QRhiTexture::RG32UI:
      return QStringLiteral("rg32ui");
    case QRhiTexture::RGBA32UI:
      return QStringLiteral("rgba32ui");
    case QRhiTexture::Format::D32FS8:
      // ???
      break;
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    case QRhiTexture::Format::R8SI:
      return QStringLiteral("r8i");
    case QRhiTexture::Format::R32SI:
      return QStringLiteral("r32i");
    case QRhiTexture::RG32SI:
      return QStringLiteral("rg32i");
    case QRhiTexture::RGBA32SI:
      return QStringLiteral("rgba32i");
#endif

    case QRhiTexture::Format::D16:
      return QStringLiteral("r16");
    case QRhiTexture::Format::D32F:
      return QStringLiteral("r32f");
      break;
    case QRhiTexture::UnknownFormat:
    default:
      break;
  }
  return QStringLiteral("rgba8");
}


RenderedCSFNode::RenderedCSFNode(const ISFNode& node) noexcept
    : NodeRenderer{node}
    , n{const_cast<ISFNode&>(node)}
{
}

RenderedCSFNode::~RenderedCSFNode() { }

TextureRenderTarget RenderedCSFNode::renderTargetForInput(const Port& p)
{
  auto it = m_rts.find(&p);
  if(it != m_rts.end())
    return it->second;
  return {};
}

std::vector<Sampler> RenderedCSFNode::allSamplers() const noexcept
{
  return m_inputSamplers;
}

struct is_output
{
  bool operator()(const isf::storage_input& v) { return v.access != "read_only"; }
  bool operator()(const isf::csf_image_input& v) { return v.access != "read_only"; }
  bool operator()(const auto& v) { return false; }
};

struct port_indices
{
  int inlet_i = 0;
  int outlet_i = 0;
  void operator()(const isf::storage_input& v)
  {
    if(v.access == "read_only")
      inlet_i++;
    else
    {
      inlet_i++;
      outlet_i++;
    }
  }
  void operator()(const isf::csf_image_input& v)
  {
    if(v.access == "read_only")
      inlet_i++;
    else
      outlet_i++;
  }
  void operator()(const auto& v) { inlet_i++; }
};
QSize RenderedCSFNode::computeTextureSize(
    const isf::csf_image_input& pass) const noexcept
{
  ossia::math_expression e;
  ossia::small_pod_vector<double, 16> data;

  const auto& desc = n.descriptor();
  // Note : reserve is super important here,
  // as the expression parser takes *references* to the
  // variables.
  data.reserve(2 + 2 * m_inputSamplers.size() + desc.inputs.size());

  int input_image_index = 0;
  for(auto& img : desc.inputs)
  {
    if(auto tex_input = ossia::get_if<isf::texture_input>(&img.data))
    {
      SCORE_ASSERT(input_image_index < m_inputSamplers.size());
      auto [s, t] = this->m_inputSamplers[input_image_index];
      QSize tex_sz = t ? t->pixelSize() : QSize{1280, 720};
      e.add_constant(
          fmt::format("var_WIDTH_{}", img.name), data.emplace_back(tex_sz.width()));
      e.add_constant(
          fmt::format("var_HEIGHT_{}", img.name), data.emplace_back(tex_sz.height()));

      input_image_index++;
    }
    else if(auto img_input = ossia::get_if<isf::csf_image_input>(&img.data))
    {
      if(img_input->access == "read_only")
      {
        SCORE_ASSERT(input_image_index < m_inputSamplers.size());
        auto [s, t] = this->m_inputSamplers[input_image_index];
        QSize tex_sz = t ? t->pixelSize() : QSize{1280, 720};
        e.add_constant(
            fmt::format("var_WIDTH_{}", img.name), data.emplace_back(tex_sz.width()));
        e.add_constant(
            fmt::format("var_HEIGHT_{}", img.name), data.emplace_back(tex_sz.height()));

        input_image_index++;
      }
    }
  }

  int inlet_k = 0;
  for(const isf::input& input : desc.inputs)
  {
    if(ossia::visit(is_output{}, input.data))
    {
      continue;
    }

    auto port = n.input[inlet_k];
    if(ossia::get_if<isf::float_input>(&input.data))
    {
      e.add_constant("var_" + input.name, data.emplace_back(*(float*)port->value));
    }
    else if(ossia::get_if<isf::long_input>(&input.data))
    {
      e.add_constant("var_" + input.name, data.emplace_back(*(int*)port->value));
    }

    inlet_k++;
  }

  QSize res;
  if(auto expr = pass.width_expression; !expr.empty())
  {
    boost::algorithm::replace_all(expr, "$", "var_");
    e.register_symbol_table();
    bool ok = e.set_expression(expr);
    if(ok)
      res.setWidth(e.value());
    else
      qDebug() << e.error().c_str() << expr.c_str();
  }
  if(auto expr = pass.height_expression; !expr.empty())
  {
    boost::algorithm::replace_all(expr, "$", "var_");
    e.register_symbol_table();
    bool ok = e.set_expression(expr);
    if(ok)
      res.setHeight(e.value());
    else
      qDebug() << e.error().c_str() << expr.c_str();
  }

  return res;
}

std::optional<QSize>
RenderedCSFNode::getImageSize(const isf::csf_image_input& img) const noexcept
{
  // 1. Does img have a width or height expression set
  if(!img.width_expression.empty() || !img.height_expression.empty())
  {
    return computeTextureSize(img);
  }

  // 2. If not: take size of first input image if any?
  if(!this->m_inputSamplers.empty())
  {
    if(this->m_inputSamplers[0].texture)
    {
      return this->m_inputSamplers[0].texture->pixelSize();
    }
  }

  // 3. If not: take size of renderer
  return std::nullopt;
}

int RenderedCSFNode::calculateStorageBufferSize(std::span<const isf::storage_input::layout_field> layout, int arrayCount) const
{
  if(layout.empty() || arrayCount <= 0)
    return 0;
    
  int totalSize = 0;
  
  for(const auto& field : layout)
  {
    int fieldSize = 0;
    QString type = QString::fromStdString(field.type);
    
    // Handle basic GLSL types according to std430 layout rules
    if(type == "float") fieldSize = 4;
    else if(type == "vec2") fieldSize = 8;
    else if(type == "vec3") fieldSize = 12;  // Note: vec3 has 4-byte alignment but 12-byte size
    else if(type == "vec4") fieldSize = 16;
    else if(type == "int") fieldSize = 4;
    else if(type == "ivec2") fieldSize = 8;
    else if(type == "ivec3") fieldSize = 12;
    else if(type == "ivec4") fieldSize = 16;
    else if(type == "bool") fieldSize = 4;   // bool is stored as int in GLSL
    else if(type == "mat2") fieldSize = 16;  // 2x2 matrix = 2 vec2s
    else if(type == "mat3") fieldSize = 36;  // 3x3 matrix = 3 vec3s, but aligned to vec4
    else if(type == "mat4") fieldSize = 64;  // 4x4 matrix = 4 vec4s
    else if(type.endsWith("[]"))
    {
      // flexible arrays are handled by arrayCount
      QString baseType = type.left(type.length() - 2);
      // Recursive call for array element size
      isf::storage_input::layout_field singleField[1] = {{field.name, baseType.toStdString()}};
      fieldSize = calculateStorageBufferSize(singleField, 1);
    }
    else
    {
      // Check if it's a custom type from the TYPES array
      bool foundCustomType = false;
      for(const auto& typeDef : n.descriptor().types)
      {
        if(QString::fromStdString(typeDef.name) == type)
        {
          fieldSize = calculateStorageBufferSize(typeDef.layout, 1);
          foundCustomType = true;
          break;
        }
      }
      
      if(!foundCustomType)
      {
        // Unknown type, assume 16 bytes (vec4 equivalent) as fallback
        qWarning() << "Unknown CSF field type:" << type << "assuming 16 bytes";
        fieldSize = 16;
      }
    }
    
    // Apply std430 alignment rules
    int alignment = std::min(fieldSize, 16); // std430 max alignment is 16 bytes
    totalSize = (totalSize + alignment - 1) & ~(alignment - 1); // Align to boundary
    totalSize += fieldSize;
  }
  
  // Align the whole struct to 16 bytes (vec4 boundary) for array elements
  totalSize = (totalSize + 15) & ~15;
  
  return totalSize * arrayCount;
}

BufferView RenderedCSFNode::createStorageBuffer(
    RenderList& renderer, const QString& name, const QString& access, int size)
{
  QRhi& rhi = *renderer.state.rhi;
  QRhiBuffer* buffer = rhi.newBuffer(
      QRhiBuffer::Static, QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer, size);

  if(buffer)
  {
    buffer->setName(QStringLiteral("CSF_StorageBuffer_%1").arg(name).toLocal8Bit());
    if(!buffer->create())
    {
      qWarning() << "Failed to create storage buffer" << name << "of size" << size;
      delete buffer;
      buffer = nullptr;
    }
  }

  return {buffer, 0, size};
}

int RenderedCSFNode::getArraySizeFromUI(const QString& bufferName) const
{
  // ISFNode automatically creates ports for storage buffers with flexible arrays
  // Look for the corresponding input in the descriptor and find its port

  port_indices p;

  int storageSizeInputIndex = -1;
  const std::string& name = bufferName.toStdString();
  for(std::size_t i = 0; i < n.m_descriptor.inputs.size(); i++)
  {
    const auto& input = n.m_descriptor.inputs[i];

    if(input.name == name)
    {
      if(auto* storage = ossia::get_if<isf::storage_input>(&input.data))
      {
        // Check if this storage buffer has flexible arrays
        for(const auto& field : storage->layout)
        {
          if(field.type.find("[]") != std::string::npos)
          {
            storageSizeInputIndex = p.inlet_i;
            break;
          }
        }
        break;
      }
    }

    ossia::visit(p, input.data);
  }

  if(storageSizeInputIndex >= 0)
  {
    // ISFNode creates ports in order of inputs, plus one extra port for array size if needed
    if(storageSizeInputIndex < n.input.size() && n.input[storageSizeInputIndex]->value)
    {
      int arraySize = *(int*)n.input[storageSizeInputIndex]->value;
      return std::max(1, arraySize); // Ensure at least 1 element
    }
  }
  
  // Default array size if not found
  return 1024;
}

BufferView RenderedCSFNode::bufferForOutput(const Port& output)
{
  for(auto& [port, index] : this->m_outStorageBuffers) {
    if(&output == port) {
      auto handle = this->m_storageBuffers[index].buffer;
      return {handle, 0, handle->size()};
    }
  }
  return {};
}

void RenderedCSFNode::updateStorageBuffers(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  bool buffersChanged = false;
  
  // Check each storage buffer to see if it needs resizing
  for(auto& storageBuffer : m_storageBuffers)
  {
    // Get current array size from UI
    int currentArraySize = getArraySizeFromUI(storageBuffer.name);

    // Calculate required buffer size
    int requiredSize
        = calculateStorageBufferSize(storageBuffer.layout, currentArraySize);

    // Check if buffer needs to be resized
    if(requiredSize != storageBuffer.lastKnownSize || !storageBuffer.buffer)
    {
      // Delete old buffer if it exists
      if(storageBuffer.buffer)
      {
        storageBuffer.buffer->deleteLater();
        storageBuffer.buffer = nullptr;
      }
      
      // Create new buffer with correct size
      if(storageBuffer.buffer)
      {
        storageBuffer.buffer->destroy();
        storageBuffer.buffer->setSize(requiredSize);
        storageBuffer.buffer->create();
      }
      else
      {
        storageBuffer.buffer
            = createStorageBuffer(
                  renderer, storageBuffer.name, storageBuffer.access, requiredSize)
                  .handle;
      }
      storageBuffer.size = requiredSize;
      storageBuffer.lastKnownSize = requiredSize;
      
      if(storageBuffer.buffer)
      {
        // Initialize buffer with zero data for predictable behavior
        QByteArray zeroData(requiredSize, 0);
        score::gfx::uploadStaticBufferWithStoredData(&res, storageBuffer.buffer, 0, std::move(zeroData));
      }
      
      buffersChanged = true;
    }
  }
  
  // If buffers changed, we need to recreate the SRBs
  if(buffersChanged)
  {
    recreateShaderResourceBindings(renderer, res);
  }
}

void RenderedCSFNode::initComputePass(
    const TextureRenderTarget& rt, RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res)
{
  QRhi& rhi = *renderer.state.rhi;
  
  if(!m_computePipeline)
  {
    createComputePipeline(renderer);
  }
  
  if(!m_computePipeline)
    return;
    
  // Ensure storage buffers are created before setting up bindings
  updateStorageBuffers(renderer, res);

  // Create shader resource bindings
  QList<QRhiShaderResourceBinding> bindings;
  
  // Binding 0: Renderer UBO (part of ProcessUBO in defaultUniforms)
  bindings.append(QRhiShaderResourceBinding::uniformBuffer(
      0, QRhiShaderResourceBinding::ComputeStage, &renderer.outputUBO()));

  // Binding 1: Process UBO (time, passIndex, etc.)
  // Per-pass: actual pointer will be set later
  bindings.append(
      QRhiShaderResourceBinding::uniformBuffer(
          1, QRhiShaderResourceBinding::ComputeStage, nullptr));

  // Binding 2: Material UBO (custom inputs)
  int bindingIndex = 2;
  if(m_materialUBO)
  {
    bindings.append(QRhiShaderResourceBinding::uniformBuffer(
        bindingIndex++, QRhiShaderResourceBinding::ComputeStage, m_materialUBO));
  }

  int input_port_index = 0;
  int input_image_index = 0;
  int output_port_index = 0;
  int output_image_index = 0;
  // Process all resources in the order they appear in the descriptor
  // This ensures the binding indices match what the shader expects
  for(const auto& input : n.m_descriptor.inputs)
  {
    // Storage buffers
    if(ossia::get_if<isf::storage_input>(&input.data))
    {
      // Find the corresponding storage buffer
      auto it = std::find_if(m_storageBuffers.begin(), m_storageBuffers.end(),
          [&input](const StorageBuffer& sb) { 
            return sb.name == QString::fromStdString(input.name); 
          });
      
      if(it != m_storageBuffers.end() && it->buffer)
      {
        if(it->access == "read_only")
        {
          QRhiBuffer* buf = it->buffer; // Default dummy buffer
          auto port = this->node.input[input_port_index];
          if(!port->edges.empty())
          {
            auto input_buf = renderer.bufferForInput(*port->edges.front());
            if(input_buf)
            {
              buf = input_buf.handle;
            }
          }
          bindings.append(
              QRhiShaderResourceBinding::bufferLoad(
                  bindingIndex++, QRhiShaderResourceBinding::ComputeStage, buf));
          input_port_index++;
        }
        else if(it->access == "write_only")
        {
          bindings.append(QRhiShaderResourceBinding::bufferStore(
              bindingIndex++, QRhiShaderResourceBinding::ComputeStage, 
              it->buffer));
          output_port_index++;
        }
        else // read_write
        {
          bindings.append(QRhiShaderResourceBinding::bufferLoadStore(
              bindingIndex++, QRhiShaderResourceBinding::ComputeStage, 
              it->buffer));
          output_port_index++;
        }
      }
      else
      {
        if(!it->buffer) {
          qDebug() << "CSF: cannot bind null buffer";
        }
      }
    }
    // Regular textures (sampled)
    else if(ossia::get_if<isf::texture_input>(&input.data))
    {
      // Regular sampled textures from m_inputSamplers
      SCORE_ASSERT(input_image_index < m_inputSamplers.size());
      auto [sampler, tex] = m_inputSamplers[input_image_index];
      SCORE_ASSERT(sampler);
      SCORE_ASSERT(tex);
      bindings.append(
          QRhiShaderResourceBinding::sampledTexture(
              bindingIndex++, QRhiShaderResourceBinding::ComputeStage, tex, sampler));
      input_port_index++;
      input_image_index++;
    }
    // CSF storage images
    else if(auto image = ossia::get_if<isf::csf_image_input>(&input.data))
    {
      // Find the corresponding storage image
      auto it = std::find_if(m_storageImages.begin(), m_storageImages.end(),
          [&input](const StorageImage& si) { 
            return si.name == QString::fromStdString(input.name); 
          });
      
      if(it != m_storageImages.end())
      {
        if(it->access == "read_only")
        {
          SCORE_ASSERT(input_image_index < m_inputSamplers.size());
          auto [sampler, tex] = m_inputSamplers[input_image_index];
          SCORE_ASSERT(sampler);
          SCORE_ASSERT(tex);

          bindings.append(
              QRhiShaderResourceBinding::imageLoad(
                  bindingIndex++, QRhiShaderResourceBinding::ComputeStage, tex, 0));

          input_port_index++;
          input_image_index++;
        }
        else
        {
          QRhiTexture::Format format
              = getTextureFormat(QString::fromStdString(image->format));
          QSize imageSize = renderer.state.renderSize;
          if(auto sz = getImageSize(*image))
            imageSize = *sz;
          if(imageSize.width() < 1 || imageSize.height() < 1)
            imageSize = renderer.state.renderSize;

          QRhiTexture::Flags flags
              = QRhiTexture::RenderTarget | QRhiTexture::UsedWithLoadStore
                | QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips;

          QRhiTexture* texture = rhi.newTexture(format, imageSize, 1, flags);
          texture->setName(("RenderedCSFNode::storageImage::" + input.name).c_str());

          if(texture && texture->create())
          {
            // If this is the first write-only or read-write image, use it as the output
            if(!m_outputTexture)
            {
              m_outputTexture = texture;
              m_outputFormat = format;
            }
            it->texture = texture;
          }
          else
          {
            delete texture;
          }

          if(it->access == "write_only" && it->texture)
          {
            bindings.append(
                QRhiShaderResourceBinding::imageStore(
                    bindingIndex++, QRhiShaderResourceBinding::ComputeStage, it->texture,
                    0));
          }
          else if(it->access == "read_write" && it->texture)
          {
            bindings.append(
                QRhiShaderResourceBinding::imageLoadStore(
                    bindingIndex++, QRhiShaderResourceBinding::ComputeStage, it->texture,
                    0));
          }
          output_port_index++;
          output_image_index++;
        }
      }
    }
    else
    {
      input_port_index++;
    }
  }

  // Set the SRB on the pipeline and create it
  {
    QRhiShaderResourceBindings* passSRB{};
    // Create one ComputePass entry for each CSF pass, all sharing the same pipeline
    // but each with their own ProcessUBO and SRB
    for(std::size_t passIdx = 0; passIdx < n.m_descriptor.csf_passes.size(); passIdx++)
    {
      // Create a separate ProcessUBO for this pass
      QRhiBuffer* passProcessUBO = rhi.newBuffer(
          QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
      passProcessUBO->setName(QStringLiteral("RenderedCSFNode::pass%1::processUBO")
                                  .arg(passIdx)
                                  .toLocal8Bit());
      if(!passProcessUBO->create())
      {
        qWarning() << "Failed to create ProcessUBO for CSF pass" << passIdx;
        delete passProcessUBO;
        continue;
      }

      // Create separate SRB for this pass with the specific ProcessUBO
      passSRB = rhi.newShaderResourceBindings();
      passSRB->setName(QString("passSRB.%1").arg(passIdx).toUtf8());

      // Replace the ProcessUBO binding (binding 1) with this pass's ProcessUBO
      // We know binding 1 is the ProcessUBO because we created it that way
      {
        bindings[1] = QRhiShaderResourceBinding::uniformBuffer(
            1, QRhiShaderResourceBinding::ComputeStage, passProcessUBO);
      }

      passSRB->setBindings(bindings.cbegin(), bindings.cend());
      if(!passSRB->create())
      {
        qWarning() << "Failed to create SRB for CSF pass" << passIdx;
        delete passSRB;
        delete passProcessUBO;
        passSRB = nullptr;
        continue;
      }

      m_computePasses.emplace_back(
          &edge, ComputePass{m_computePipeline, passSRB, passProcessUBO});
    }

    if(!passSRB)
      return;
    // We set any passSRB - they are all the same, the only change is the processUBO.
    m_computePipeline->setShaderResourceBindings(passSRB);
    m_computePipeline->create();

    if(rt.renderTarget)
    {
      // Also create the graphics pass for rendering the compute output
      createGraphicsPass(rt, renderer, edge, res);
      // FIXME we need to do this for every out!
    }
  }
}

void RenderedCSFNode::createGraphicsPass(
    const TextureRenderTarget& rt, RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res)
{
  // Create a graphics pass to render our compute output texture to the render target
  static const constexpr auto vertex_shader = R"_(#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;

layout(location = 0) out vec2 v_texcoord;

layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
  v_texcoord = texcoord;
  gl_Position = renderer.clipSpaceCorrMatrix * vec4(position.xy, 0.0, 1.);
#if defined(QSHADER_SPIRV) || defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  gl_Position.y = - gl_Position.y;
#endif
}
)_";

  static const constexpr auto fragment_shader_rgba = R"_(#version 450
layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

layout(binding = 3) uniform sampler2D outputTexture;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

void main() { fragColor = texture(outputTexture, v_texcoord); }
)_";
  static const constexpr auto fragment_shader_r = R"_(#version 450
layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

layout(binding = 3) uniform sampler2D outputTexture;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

void main() { fragColor = vec4(texture(outputTexture, v_texcoord).rrr, 1.0); }
)_";

  // Get the mesh for rendering a fullscreen quad
  const auto& mesh = renderer.defaultTriangle();

  // Find the texture to display - either m_outputTexture or the first write-capable storage image
  QRhiTexture* textureToRender = m_outputTexture;
  if(!textureToRender)
  {
    // Look for a write-only or read-write storage image to use as output
    for(const auto& storageImage : m_storageImages)
    {
      if(storageImage.texture
         && (storageImage.access == "write_only" || storageImage.access == "read_write"))
      {
        textureToRender = storageImage.texture;
        break;
      }
    }
  }
  // If we still don't have a texture, we can't create the graphics pass
  if(!textureToRender)
  {
    qWarning() << "No output texture available for graphics pass";
    return;
  }

  auto fmt = textureToRender->format();
  const char* fragment_shader{};
  switch(fmt)
  {
    case QRhiTexture::Format::R8:
    case QRhiTexture::Format::RED_OR_ALPHA8:
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    case QRhiTexture::Format::R8UI:
    case QRhiTexture::Format::R32UI:
#endif
    case QRhiTexture::Format::R16:
    case QRhiTexture::Format::R16F:
    case QRhiTexture::Format::R32F:
    case QRhiTexture::Format::D16:
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    case QRhiTexture::Format::D24:
    case QRhiTexture::Format::D24S8:
#endif
    case QRhiTexture::Format::D32F:
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    case QRhiTexture::Format::D32FS8:
#endif
      fragment_shader = fragment_shader_r;
      break;
    default:
      fragment_shader = fragment_shader_rgba;
      break;
  }

  // Compile shaders
  auto [vertexS, fragmentS] = score::gfx::makeShaders(renderer.state, vertex_shader, fragment_shader);

  // Create a sampler for our output texture
  QRhiSampler* outputSampler = renderer.state.rhi->newSampler(
    QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
    QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
  outputSampler->setName("RenderedCSFNode::OutputSampler");
  outputSampler->create();
    
  // Initialize mesh buffers
  MeshBuffers meshBuffers = renderer.initMeshBuffer(mesh, res);
  
  // Build the pipeline to render our compute result
  auto pip = score::gfx::buildPipeline(
      renderer, mesh, vertexS, fragmentS, rt, nullptr, nullptr, 
      std::array<Sampler, 1>{Sampler{outputSampler, textureToRender}});
      
  if(pip.pipeline)
  {
    m_graphicsPasses.emplace_back(&edge, GraphicsPass{pip, outputSampler, meshBuffers});
  }
  else
  {
    delete outputSampler;
  }
}

QString RenderedCSFNode::updateShaderWithImageFormats(QString current)
{
  int sampler_index = 0;
  for(const auto& input : n.m_descriptor.inputs)
  {
    if(auto tex_input = ossia::get_if<isf::texture_input>(&input.data))
    {
      sampler_index++;
    }
    if(auto image = ossia::get_if<isf::csf_image_input>(&input.data))
    {
      if(image->access == "read_only")
      {
        SCORE_ASSERT(sampler_index < m_inputSamplers.size());
        auto tex_n = m_inputSamplers[sampler_index].texture;
        if(!tex_n)
          return current;

        const auto fmt = tex_n->format();
        const auto layout_fmt = rhiTextureFormatToShaderLayoutFormatString(fmt);

        const auto before = QStringLiteral(", rgba8) readonly uniform image2D %1;").arg(input.name.c_str());
        const auto after = QStringLiteral(", %1) readonly uniform image2D %2;").arg(layout_fmt).arg(input.name.c_str());

        current.replace(before, after);
        sampler_index++;
      }
    }
  }
  return current;

}

void RenderedCSFNode::createComputePipeline(RenderList& renderer)
{
  QRhi& rhi = *renderer.state.rhi;
  
  if(!rhi.isFeatureSupported(QRhi::Compute))
  {
    qWarning() << "Compute shaders not supported on this backend";
    return;
  }
  
  try
  {
    // Compile the compute shader
    // Here we need to hot-patch the image format with the one we're getting at run-time
    // as it has to be specified in the shader code:
    // layout(binding = 3, rgba8) readonly uniform image2D inputImage;
    const auto shader = updateShaderWithImageFormats(n.m_computeS);
    QShader computeShader = score::gfx::makeCompute(renderer.state, shader);
    
    // Create compute pipeline but don't create() it yet - we need SRB first
    m_computePipeline = rhi.newComputePipeline();
    m_computePipeline->setShaderStage(
        QRhiShaderStage(QRhiShaderStage::Compute, computeShader));
    
    // We'll create the pipeline later when we have the shader resource bindings
    m_computeShader = computeShader;
  }
  catch(const std::exception& e)
  {
    qWarning() << "Failed to create compute shader:" << e.what();
    m_computePipeline = nullptr;
  }
}

void RenderedCSFNode::init(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  QRhi& rhi = *renderer.state.rhi;

  // Check for compute support
  if(!rhi.isFeatureSupported(QRhi::Compute))
  {
    qWarning() << "Compute shaders not supported on this backend";
    return;
  }
  
  // ProcessUBO will be created per-pass in initComputePass
  
  // Create the material UBO
  m_materialSize = n.m_materialSize;
  if(m_materialSize > 0)
  {
    m_materialUBO = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, m_materialSize);
    m_materialUBO->setName("RenderedCSFNode::init::m_materialUBO");
    if(!m_materialUBO->create())
    {
      qWarning() << "Failed to create uniform buffer";
      delete m_materialUBO;
      m_materialUBO = nullptr;
    }
  }

  // Initialize input samplers
  SCORE_ASSERT(m_rts.empty());
  SCORE_ASSERT(m_computePasses.empty());
  SCORE_ASSERT(m_inputSamplers.empty());

  // Create samplers for input textures
  m_inputSamplers = initInputSamplers(this->n, renderer, n.input, m_rts);

  // Parse descriptor to create storage buffers and determine output texture requirements
  
  int sb_index = 0;
  int outlet_index = 0;
  auto& outlets = n.output;
  for(const auto& input : n.m_descriptor.inputs)
  {
    // Handle storage buffers
    if(auto* storage = ossia::get_if<isf::storage_input>(&input.data))
    {
      // Create storage buffer entry - actual buffer will be created/sized in updateStorageBuffers
      StorageBuffer sb;
      sb.buffer = nullptr; // Will be created in updateStorageBuffers
      sb.size = 0;
      sb.lastKnownSize = 0; // Force initial creation
      sb.name = QString::fromStdString(input.name);
      sb.access = QString::fromStdString(storage->access);
      sb.layout = storage->layout; // Store layout for size calculation
      m_storageBuffers.push_back(sb);

      if(sb.access.contains("write")) {
        m_outStorageBuffers.push_back({outlets[outlet_index], sb_index});
        outlet_index++;
      }
      sb_index++;
    }
    // Handle CSF images
    else if(auto* image = ossia::get_if<isf::csf_image_input>(&input.data))
    {
      QRhiTexture::Format format = getTextureFormat(QString::fromStdString(image->format));
      m_storageImages.push_back(
          StorageImage{
              nullptr, QString::fromStdString(input.name),
              QString::fromStdString(image->access), format});

      if(m_storageImages.back().access.contains("write")) {
        outlet_index++;
      }
    }
  }

  m_outputTexture = nullptr;

  // Create the compute passes for each output edge
  for(Edge* edge : n.output[0]->edges)
  {
    const auto& rt = renderer.renderTargetForOutput(*edge);
    initComputePass(rt, renderer, *edge, res);
  }
}

void RenderedCSFNode::update(
    RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge)
{
  // Update standard ProcessUBO (time, renderSize, etc.)
  // passIndex will be set per-pass in runInitialPasses
  n.standardUBO.frameIndex++;
  if(edge)
  {
    auto sz = renderer.renderSize(edge);
    n.standardUBO.renderSize[0] = sz.width();
    n.standardUBO.renderSize[1] = sz.height();
  }
  
  // Update ProcessUBO for each compute pass with the correct passIndex
  std::size_t passIdx = 0;
  for(auto& [edge, pass] : m_computePasses)
  {
    if(pass.processUBO)
    {
      // Set the correct passIndex for this CSF pass
      n.standardUBO.passIndex = static_cast<int32_t>(passIdx);
      res.updateDynamicBuffer(pass.processUBO, 0, sizeof(ProcessUBO), &n.standardUBO);
      passIdx++;
    }
  }
  
  // Update storage buffers (check for size changes and reallocate if needed)
  updateStorageBuffers(renderer, res);
  
  // Update uniform buffer with current input values
  if(m_materialUBO && n.m_material_data)
  {
    res.updateDynamicBuffer(m_materialUBO, 0, m_materialSize, n.m_material_data.get());
  }

  for(auto& [sampler, texture] : this->m_inputSamplers)
  {
    res.generateMips(texture);
  }
  
  // Update output texture size if it has changed
  // TODO: Check if texture size inputs have changed and recreate texture if needed
}

void RenderedCSFNode::recreateShaderResourceBindings(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  QRhi& rhi = *renderer.state.rhi;
  
  // Build the bindings list (same as in initComputePass)
  QList<QRhiShaderResourceBinding> bindings;
  
  // Binding 0: Renderer UBO
  bindings.append(QRhiShaderResourceBinding::uniformBuffer(
      0, QRhiShaderResourceBinding::ComputeStage, &renderer.outputUBO()));

  // Binding 1: Process UBO (will be set per-pass)
  bindings.append(
      QRhiShaderResourceBinding::uniformBuffer(
          1, QRhiShaderResourceBinding::ComputeStage, nullptr));

  // Binding 2: Material UBO (custom inputs)
  int bindingIndex = 2;
  if(m_materialUBO)
  {
    bindings.append(QRhiShaderResourceBinding::uniformBuffer(
        bindingIndex++, QRhiShaderResourceBinding::ComputeStage, m_materialUBO));
  }

  int input_port_index = 0;
  int input_image_index = 0;
  int output_port_index = 0;
  int output_image_index = 0;

  // Process all resources in the order they appear in the descriptor
  for(const auto& input : n.m_descriptor.inputs)
  {
    // Storage buffers
    if(ossia::get_if<isf::storage_input>(&input.data))
    {
      // Find the corresponding storage buffer
      auto it = std::find_if(m_storageBuffers.begin(), m_storageBuffers.end(),
          [&input](const StorageBuffer& sb) { 
            return sb.name == QString::fromStdString(input.name); 
          });
      
      if(it != m_storageBuffers.end() && it->buffer)
      {
        if(it->access == "read_only")
        {
          QRhiBuffer* buf = it->buffer; // Default dummy buffer
          auto port = this->node.input[input_port_index];
          if(!port->edges.empty())
          {
            auto input_buf = renderer.bufferForInput(*port->edges.front());
            if(input_buf)
            {
              buf = input_buf.handle;
            }
          }
          bindings.append(
              QRhiShaderResourceBinding::bufferLoad(
                  bindingIndex++, QRhiShaderResourceBinding::ComputeStage, buf));
          input_port_index++;
        }
        else if(it->access == "write_only")
        {
          bindings.append(QRhiShaderResourceBinding::bufferStore(
              bindingIndex++, QRhiShaderResourceBinding::ComputeStage, 
              it->buffer));
          output_port_index++;
        }
        else // read_write
        {
          bindings.append(QRhiShaderResourceBinding::bufferLoadStore(
              bindingIndex++, QRhiShaderResourceBinding::ComputeStage, 
              it->buffer));
          output_port_index++;
        }
      }
    }
    // Regular textures (sampled)
    else if(ossia::get_if<isf::texture_input>(&input.data))
    {
      // Regular sampled textures from m_inputSamplers
      if(input_image_index < m_inputSamplers.size())
      {
        auto [sampler, tex] = m_inputSamplers[input_image_index];
        if(sampler && tex)
        {
          bindings.append(
              QRhiShaderResourceBinding::sampledTexture(
                  bindingIndex++, QRhiShaderResourceBinding::ComputeStage, tex, sampler));
        }
      }
      input_port_index++;
      input_image_index++;
    }
    // CSF storage images
    else if(auto image = ossia::get_if<isf::csf_image_input>(&input.data))
    {
      // Find the corresponding storage image
      auto it = std::find_if(m_storageImages.begin(), m_storageImages.end(),
          [&input](const StorageImage& si) { 
            return si.name == QString::fromStdString(input.name); 
          });
      
      if(it != m_storageImages.end())
      {
        if(it->access == "read_only")
        {
          if(input_image_index < m_inputSamplers.size())
          {
            auto [sampler, tex] = m_inputSamplers[input_image_index];
            if(sampler && tex)
            {
              bindings.append(
                  QRhiShaderResourceBinding::imageLoad(
                      bindingIndex++, QRhiShaderResourceBinding::ComputeStage, tex, 0));
            }
          }
          input_port_index++;
          input_image_index++;
        }
        else if(it->texture)
        {
          if(it->access == "write_only")
          {
            bindings.append(
                QRhiShaderResourceBinding::imageStore(
                    bindingIndex++, QRhiShaderResourceBinding::ComputeStage, it->texture,
                    0));
          }
          else if(it->access == "read_write")
          {
            bindings.append(
                QRhiShaderResourceBinding::imageLoadStore(
                    bindingIndex++, QRhiShaderResourceBinding::ComputeStage, it->texture,
                    0));
          }
          output_port_index++;
          output_image_index++;
        }
      }
    }
    else
    {
      input_port_index++;
    }
  }
  
  // Recreate SRBs for each compute pass
  for(auto& [edge, pass] : m_computePasses)
  {
    if(pass.srb)
    {
      // Delete old SRB
      delete pass.srb;
      pass.srb = nullptr;
    }
    
    // Create new SRB
    pass.srb = rhi.newShaderResourceBindings();
    
    // Set the ProcessUBO binding for this pass
    if(pass.processUBO)
    {
      bindings[1] = QRhiShaderResourceBinding::uniformBuffer(
          1, QRhiShaderResourceBinding::ComputeStage, pass.processUBO);
    }
    
    pass.srb->setBindings(bindings.cbegin(), bindings.cend());
    if(!pass.srb->create())
    {
      qWarning() << "Failed to recreate SRB for compute pass";
      delete pass.srb;
      pass.srb = nullptr;
    }
  }
  
  // Update the pipeline with one of the SRBs (they're all compatible)
  if(!m_computePasses.empty() && m_computePasses[0].second.srb)
  {
    m_computePipeline->setShaderResourceBindings(m_computePasses[0].second.srb);
  }
}

void RenderedCSFNode::release(RenderList& r)
{
  // Clean up compute passes
  for(auto& [edge, pass] : m_computePasses)
  {
    delete pass.srb;
    if(pass.processUBO)
    {
      pass.processUBO->deleteLater();
    }
  }
  m_computePasses.clear();
  
  // Clean up graphics passes
  for(auto& [edge, pass] : m_graphicsPasses)
  {
    pass.pipeline.release();
    delete pass.outputSampler;
  }
  m_graphicsPasses.clear();
  
  // Clean up pipeline
  delete m_computePipeline;
  m_computePipeline = nullptr;
  
  // Clean up storage buffers
  for(auto& storageBuffer : m_storageBuffers)
  {
    r.releaseBuffer(storageBuffer.buffer);
  }
  m_storageBuffers.clear();
  
  // Clean up storage images
  for(auto& storageImage : m_storageImages)
  {
    if(storageImage.texture)
    {
      storageImage.texture->deleteLater();
    }
  }
  m_storageImages.clear();
  
  // Clean up buffers and textures
  delete m_materialUBO;
  m_materialUBO = nullptr;

  // Clean up samplers
  for(auto sampler : m_inputSamplers)
  {
    delete sampler.sampler;
    // texture isdeleted elsewhere
  }
  m_inputSamplers.clear();
  for(auto [edge, rt] : m_rts)
  {
    rt.release();
  }
  m_rts.clear();
}

void RenderedCSFNode::runRenderPass(
    RenderList& renderer, QRhiCommandBuffer& commands, Edge& edge)
{
  // Find the graphics pass for this edge
  auto pass_it = ossia::find_if(
      m_graphicsPasses, [&](const auto& p) { return p.first == &edge; });
  
  if(pass_it == m_graphicsPasses.end())
    return;
    
  const auto& graphicsPass = pass_it->second;
  
  // Get the render target for this edge
  auto rt = renderer.renderTargetForOutput(edge);
  if(!rt.renderTarget)
    return;
    
  // Render the fullscreen quad with our compute result
  commands.setGraphicsPipeline(graphicsPass.pipeline.pipeline);
  commands.setShaderResources(graphicsPass.pipeline.srb);
  commands.setViewport(QRhiViewport(0, 0, rt.texture->pixelSize().width(), rt.texture->pixelSize().height()));
  
  const auto& mesh = renderer.defaultTriangle();
  mesh.draw(graphicsPass.meshBuffers, commands);
}

void RenderedCSFNode::runInitialPasses(
    RenderList& renderer, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res,
    Edge& edge)
{
  // Run all passes sequentially
  for(std::size_t passIndex = 0; passIndex < n.m_descriptor.csf_passes.size(); passIndex++)
  {
    const auto& passDesc = n.m_descriptor.csf_passes[passIndex];
    
    // Find the compute pass for this specific CSF pass index
    // Since we created one ComputePass per CSF pass, we can index directly
    if(passIndex >= m_computePasses.size())
    {
      qWarning() << "CSF pass index" << passIndex << "exceeds available compute passes" << m_computePasses.size();
      continue;
    }
    
    const auto& pass = m_computePasses[passIndex].second;

    // Begin compute pass (outside of any render pass)
    commands.beginComputePass(res);
    res = nullptr;

    // Set compute pipeline
    commands.setComputePipeline(pass.pipeline);
    
    // Set shader resources
    commands.setShaderResources(pass.srb);
    
    // Calculate dispatch size based on pass configuration
    
    // Use pass-specific local sizes
    int localX = passDesc.local_size[0];
    int localY = passDesc.local_size[1];
    int localZ = passDesc.local_size[2];
    
    int dispatchX{}, dispatchY{}, dispatchZ{};
    
    // Calculate dispatch size based on execution model
    if(passDesc.execution_type == "2D_IMAGE")
    {
      // For 2D image execution, dispatch based on image size and workgroup size
      QSize textureSize = m_outputTexture ? m_outputTexture->pixelSize() : QSize(1280, 720);
      dispatchX = (textureSize.width() + localX - 1) / localX;
      dispatchY = (textureSize.height() + localY - 1) / localY;
      dispatchZ = 1;
    }
    else if(passDesc.execution_type == "MANUAL")
    {
      // For manual execution, use specified workgroups
      dispatchX = passDesc.workgroups[0];
      dispatchY = passDesc.workgroups[1];
      dispatchZ = passDesc.workgroups[2];
    }
    else if(passDesc.execution_type == "1D_BUFFER")
    {
      int n = 1;
      for(auto& [port, index] : this->m_outStorageBuffers) {
        if(port == edge.source) {
          n = this->m_storageBuffers[index].size;
          break;
        }
      }

      const auto requiredInvocations = n;
      const auto threadsPerWorkgroup = localX * localY * localZ;
      const int64_t totalWorkgroups = (requiredInvocations + threadsPerWorkgroup - 1) / threadsPerWorkgroup;
      static constexpr int64_t maxWorkgroups = 65535;

      if(totalWorkgroups > maxWorkgroups * maxWorkgroups)
      {
        dispatchX = maxWorkgroups;
        int64_t remaining = (totalWorkgroups + maxWorkgroups - 1) / maxWorkgroups;
        dispatchY = std::min(remaining, maxWorkgroups);
        dispatchZ = (remaining + maxWorkgroups - 1) / maxWorkgroups;
      }
      else if(totalWorkgroups > maxWorkgroups)
      {
        dispatchX = std::min(totalWorkgroups, maxWorkgroups);
        dispatchY = (totalWorkgroups + maxWorkgroups - 1) / maxWorkgroups;
        dispatchZ = 1;
      }
      else
      {
        dispatchX = totalWorkgroups;
        dispatchY = 1;
        dispatchZ = 1;
      }
    }
    else
    {
      // Default fallback
      QSize textureSize = m_outputTexture ? m_outputTexture->pixelSize() : QSize(1280, 720);
      dispatchX = (textureSize.width() + localX - 1) / localX;
      dispatchY = (textureSize.height() + localY - 1) / localY;
      dispatchZ = 1;
    }

    // Dispatch compute shader
    commands.dispatch(dispatchX, dispatchY, dispatchZ);

    // End compute pass
    commands.endComputePass();
  }
}
}
