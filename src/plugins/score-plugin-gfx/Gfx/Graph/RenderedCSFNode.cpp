#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/RenderedCSFNode.hpp>
#include <Gfx/Graph/RenderedISFSamplerUtils.hpp>
#include <Gfx/Graph/Utils.hpp>
#include <Gfx/Graph/CommonUBOs.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/algorithms.hpp>

namespace score::gfx
{

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

QRhiTexture::Format RenderedCSFNode::getTextureFormat(const QString& format) const
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

int RenderedCSFNode::calculateStorageBufferSize(const std::vector<isf::storage_input::layout_field>& layout, int arrayCount) const
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
      // Handle array types - for now assume flexible arrays are handled by arrayCount
      QString baseType = type.left(type.length() - 2);
      // Recursive call for array element size
      std::vector<isf::storage_input::layout_field> singleField = {{field.name, baseType.toStdString()}};
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

QRhiBuffer* RenderedCSFNode::createStorageBuffer(RenderList& renderer, const QString& name, const QString& access, int size)
{
  QRhi& rhi = *renderer.state.rhi;
  QRhiBuffer* buffer
      = rhi.newBuffer(QRhiBuffer::Static, QRhiBuffer::StorageBuffer, size);

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
  
  return buffer;
}

int RenderedCSFNode::getArraySizeFromUI(const QString& bufferName) const
{
  // ISFNode automatically creates ports for storage buffers with flexible arrays
  // Look for the corresponding input in the descriptor and find its port
  
  int storageInputIndex = -1;
  for(std::size_t i = 0; i < n.m_descriptor.inputs.size(); i++)
  {
    const auto& input = n.m_descriptor.inputs[i];
    if(QString::fromStdString(input.name) == bufferName)
    {
      if(auto* storage = ossia::get_if<isf::storage_input>(&input.data))
      {
        // Check if this storage buffer has flexible arrays
        for(const auto& field : storage->layout)
        {
          if(field.type.find("[]") != std::string::npos)
          {
            storageInputIndex = i;
            break;
          }
        }
        break;
      }
    }
  }
  
  if(storageInputIndex >= 0)
  {
    // ISFNode creates ports in order of inputs, plus one extra port for array size if needed
    int portIndex = storageInputIndex;
    
    // Count how many ports come before this storage buffer
    for(int i = 0; i < storageInputIndex; i++)
    {
      const auto& input = n.m_descriptor.inputs[i];
      if(auto* storage = ossia::get_if<isf::storage_input>(&input.data))
      {
        // Check if this storage buffer has flexible arrays - if so, it gets an extra port
        for(const auto& field : storage->layout)
        {
          if(field.type.find("[]") != std::string::npos)
          {
            portIndex++; // Extra port for array size
            break;
          }
        }
      }
    }
    
    // The array size port comes after the regular input ports
    portIndex++; // Move to the array size port

    if(portIndex < n.input.size() && n.input[portIndex]->value)
    {
      int arraySize = *(int*)n.input[portIndex]->value;
      return std::max(1, arraySize); // Ensure at least 1 element
    }
  }
  
  // Default array size if not found
  return 1024;
}

void RenderedCSFNode::updateStorageBuffers(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  // Check each storage buffer to see if it needs resizing
  for(auto& storageBuffer : m_storageBuffers)
  {
    // Get current array size from UI
    int currentArraySize = getArraySizeFromUI(storageBuffer.name);
    
    // Calculate required buffer size
    int requiredSize = calculateStorageBufferSize(storageBuffer.layout, currentArraySize);
    
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
      storageBuffer.buffer = createStorageBuffer(renderer, storageBuffer.name, storageBuffer.access, requiredSize);
      storageBuffer.size = requiredSize;
      storageBuffer.lastKnownSize = requiredSize;
      
      if(storageBuffer.buffer)
      {
        // Initialize buffer with zero data for predictable behavior
        QByteArray zeroData(requiredSize, 0);
        res.uploadStaticBuffer(storageBuffer.buffer, zeroData.constData());
      }
    }
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
          bindings.append(QRhiShaderResourceBinding::bufferLoad(
              bindingIndex++, QRhiShaderResourceBinding::ComputeStage, 
              it->buffer));
        }
        else if(it->access == "write_only")
        {
          bindings.append(QRhiShaderResourceBinding::bufferStore(
              bindingIndex++, QRhiShaderResourceBinding::ComputeStage, 
              it->buffer));
        }
        else // read_write
        {
          bindings.append(QRhiShaderResourceBinding::bufferLoadStore(
              bindingIndex++, QRhiShaderResourceBinding::ComputeStage, 
              it->buffer));
        }
      }
    }
    // Regular textures (sampled)
    else if(ossia::get_if<isf::texture_input>(&input.data))
    {
      // Regular sampled textures from m_inputSamplers
      if(!m_inputSamplers.empty() && m_inputSamplers[0].texture)
      {
        bindings.append(
            QRhiShaderResourceBinding::sampledTexture(
                bindingIndex++, QRhiShaderResourceBinding::ComputeStage,
                m_inputSamplers[0].texture, m_inputSamplers[0].sampler));
      }
    }
    // CSF storage images
    else if(ossia::get_if<isf::csf_image_input>(&input.data))
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
          // For read-only images, use the texture from the input port's render target
          QRhiTexture* inputTexture = nullptr;
          
          // Find the input port that corresponds to this read-only image
          // CSF read-only images create input ports of type Image
          // We need to find the render target that was created for this input
          
          // The issue is we need to match by name somehow
          // Let's use a simple approach: find the first Image port that has a render target
          // This is a simplification - in a real implementation we'd need better tracking
          
          for(Port* port : n.input)
          {
            if(port->type == Types::Image)
            {
              auto rtIt = m_rts.find(port);
              if(rtIt != m_rts.end() && rtIt->second.texture)
              {
                inputTexture = rtIt->second.texture;
                break; // Use the first available Image input
              }
            }
          }
          
          if(inputTexture)
          {
            bindings.append(QRhiShaderResourceBinding::imageLoad(
                bindingIndex++, QRhiShaderResourceBinding::ComputeStage, 
                inputTexture, 0));
          }
          else
          {
            // Fallback to empty texture if no input connected
            bindings.append(QRhiShaderResourceBinding::imageLoad(
                bindingIndex++, QRhiShaderResourceBinding::ComputeStage, 
                &renderer.emptyTexture(), 0));
          }
        }
        else if(it->access == "write_only" && it->texture)
        {
          bindings.append(QRhiShaderResourceBinding::imageStore(
              bindingIndex++, QRhiShaderResourceBinding::ComputeStage, 
              it->texture, 0));
        }
        else if(it->access == "read_write" && it->texture)
        {
          bindings.append(QRhiShaderResourceBinding::imageLoadStore(
              bindingIndex++, QRhiShaderResourceBinding::ComputeStage, 
              it->texture, 0));
        }
      }
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

    // Also create the graphics pass for rendering the compute output
    createGraphicsPass(rt, renderer, edge, res);
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
}
)_";

  static const constexpr auto fragment_shader = R"_(#version 450
layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

layout(binding = 3) uniform sampler2D outputTexture;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

void main()
{
  fragColor = texture(outputTexture, v_texcoord);
}
)_";

  // Get the mesh for rendering a fullscreen quad
  const auto& mesh = renderer.defaultTriangle();
  
  // Compile shaders
  auto [vertexS, fragmentS] = score::gfx::makeShaders(renderer.state, vertex_shader, fragment_shader);
  
  // Find the texture to display - either m_outputTexture or the first write-capable storage image
  QRhiTexture* textureToRender = m_outputTexture;
  if(!textureToRender)
  {
    // Look for a write-only or read-write storage image to use as output
    for(const auto& storageImage : m_storageImages)
    {
      if(storageImage.texture && 
         (storageImage.access == "write_only" || storageImage.access == "read_write"))
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
    QShader computeShader = score::gfx::makeCompute(renderer.state, n.m_computeS);
    
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
  
  // Parse descriptor to create storage buffers and determine output texture requirements
  QSize textureSize{1280, 720};   // Default size
  QString outputFormat = "RGBA8"; // Default format
  
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
    }
    // Handle CSF images
    else if(auto* image = ossia::get_if<isf::csf_image_input>(&input.data))
    {
      QRhiTexture::Format format = getTextureFormat(QString::fromStdString(image->format));
      
      // Only create storage textures for write-only and read-write images
      // Read-only images will come from input ports
      if(image->access == "write_only" || image->access == "read_write")
      {
        QSize imageSize = textureSize; // Use default size for now
        
        QRhiTexture::Flags flags = QRhiTexture::RenderTarget | QRhiTexture::UsedWithLoadStore;
        if(image->access == "read_write" || image->access == "write_only")
        {
          flags |= QRhiTexture::UsedWithGenerateMips;
        }
        
        QRhiTexture* texture = rhi.newTexture(format, imageSize, 1, flags);
        texture->setName(("RenderedCSFNode::storageImage::" + input.name).c_str());
        
        if(texture && texture->create())
        {
          m_storageImages.push_back(
              StorageImage{texture, 
                          QString::fromStdString(input.name),
                          QString::fromStdString(image->access),
                          format});
          
          // If this is the first write-only or read-write image, use it as the output
          if((image->access == "write_only" || image->access == "read_write") && !m_outputTexture)
          {
            m_outputTexture = texture;
            m_outputFormat = format;
          }
        }
        else
        {
          delete texture;
        }
      }
      else if(image->access == "read_only")
      {
        // Read-only images will be handled through input ports
        // Store a placeholder entry so we know about this image
        m_storageImages.push_back(
            StorageImage{nullptr, 
                        QString::fromStdString(input.name),
                        QString::fromStdString(image->access),
                        format});
      }
    }
  }
  
  // Create output texture with storage usage for compute shaders if not already created
  if(!m_outputTexture)
  {
    m_outputFormat = getTextureFormat(outputFormat);
    m_outputTexture = rhi.newTexture(
        m_outputFormat, textureSize, 1, 
        QRhiTexture::RenderTarget | QRhiTexture::UsedWithLoadStore | QRhiTexture::UsedWithGenerateMips);
    m_outputTexture->setName("RenderedCSFNode::outputTexture");
    if(!m_outputTexture->create())
    {
      qWarning() << "Failed to create output texture";
      delete m_outputTexture;
      m_outputTexture = nullptr;
      return;
    }
  }
  
  // Initialize input samplers
  SCORE_ASSERT(m_rts.empty());
  SCORE_ASSERT(m_computePasses.empty());
  SCORE_ASSERT(m_inputSamplers.empty());

  // Create samplers for input textures
  auto [samplers, cur_pos] = initInputSamplers(
      this->n, renderer, n.input, m_rts, n.m_material_data.get());
  m_inputSamplers = std::move(samplers);
  
  // Create the compute passes for each output edge
  for(Edge* edge : n.output[0]->edges)
  {
    auto rt = renderer.renderTargetForOutput(*edge);
    if(rt.renderTarget)
    {
      initComputePass(rt, renderer, *edge, res);
    }
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
    delete storageBuffer.buffer;
  }
  m_storageBuffers.clear();
  
  // Clean up storage images
  for(auto& storageImage : m_storageImages)
  {
    if(storageImage.texture && storageImage.texture != m_outputTexture)
    {
      storageImage.texture->deleteLater();
    }
  }
  m_storageImages.clear();
  
  // Clean up buffers and textures
  delete m_materialUBO;
  m_materialUBO = nullptr;
  
  if(m_outputTexture)
  {
    m_outputTexture->deleteLater();
    m_outputTexture = nullptr;
  }
  
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
    QSize textureSize = m_outputTexture ? m_outputTexture->pixelSize() : QSize(1280, 720);
    
    // Use pass-specific local sizes
    int workgroupX = passDesc.local_size[0];
    int workgroupY = passDesc.local_size[1];
    int workgroupZ = passDesc.local_size[2];
    (void)workgroupZ; // Currently unused but may be needed for 3D dispatches
    
    int dispatchX, dispatchY, dispatchZ;
    
    // Calculate dispatch size based on execution model
    if(passDesc.execution_type == "2D_IMAGE")
    {
      // For 2D image execution, dispatch based on image size and workgroup size
      dispatchX = (textureSize.width() + workgroupX - 1) / workgroupX;
      dispatchY = (textureSize.height() + workgroupY - 1) / workgroupY;
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
      // For 1D buffer execution, calculate based on buffer size
      // This would need buffer size information from the target resource
      dispatchX = (textureSize.width() + workgroupX - 1) / workgroupX;
      dispatchY = 1;
      dispatchZ = 1;
    }
    else
    {
      // Default fallback
      dispatchX = (textureSize.width() + workgroupX - 1) / workgroupX;
      dispatchY = (textureSize.height() + workgroupY - 1) / workgroupY;
      dispatchZ = 1;
    }

    // Dispatch compute shader
    commands.dispatch(dispatchX, dispatchY, dispatchZ);
    
    // End compute pass
    commands.endComputePass();
  }
}
}
