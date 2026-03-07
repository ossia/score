#include "ossia/detail/fmt.hpp"

#include <Gfx/Graph/CommonUBOs.hpp>
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/RenderedCSFNode.hpp>
#include <Gfx/Graph/RenderedISFSamplerUtils.hpp>
#include <Gfx/Graph/SSBO.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/dataflow/geometry_port.hpp>
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

void RenderedCSFNode::updateInputTexture(const Port& input, QRhiTexture* tex)
{
  int sampler_idx = 0;
  for(auto* p : node.input)
  {
    if(p == &input)
      break;
    if(p->type == Types::Image)
      sampler_idx++;
  }

  if(sampler_idx < (int)m_inputSamplers.size())
  {
    auto& sampl = m_inputSamplers[sampler_idx];
    if(sampl.texture != tex)
    {
      sampl.texture = tex;
      for(auto& [e, cp] : m_computePasses)
        if(cp.srb)
          score::gfx::replaceTexture(*cp.srb, sampl.sampler, tex);
      for(auto& [e, gp] : m_graphicsPasses)
        if(gp.pipeline.srb)
          score::gfx::replaceTexture(*gp.pipeline.srb, sampl.sampler, tex);
    }
  }
}

std::vector<Sampler> RenderedCSFNode::allSamplers() const noexcept
{
  return m_inputSamplers;
}

struct is_output
{
  bool operator()(const isf::storage_input& v) { return v.access != "read_only"; }
  bool operator()(const isf::csf_image_input& v) { return v.access != "read_only"; }
  bool operator()(const isf::geometry_input& v)
  {
    for(const auto& attr : v.attributes)
      if(attr.access != "read_only")
        return true;
    return false;
  }
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
  void operator()(const isf::geometry_input& v)
  {
    inlet_i++; // geometry always has an input port
    for(const auto& attr : v.attributes)
    {
      if(attr.access != "read_only")
      {
        outlet_i++; // one geometry output port if any attribute is writable
        break;
      }
    }
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
    auto requiredSize = score::gfx::calculateStorageBufferSize(
        storageBuffer.layout, currentArraySize, this->n.descriptor());
    if(requiredSize >= INT_MAX)
      requiredSize = INT_MAX;

    // Check if buffer needs to be resized
    if(requiredSize != storageBuffer.lastKnownSize || !storageBuffer.buffer)
    {
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
  // FIXME if(buffersChanged)
  {
    recreateShaderResourceBindings(renderer, res);
  }
}

// Returns the byte size of a GLSL type for SoA SSBO element stride
static int glslTypeSizeBytes(const std::string& type) noexcept
{
  if(type == "float" || type == "int" || type == "uint")
    return 4;
  if(type == "vec2" || type == "ivec2" || type == "uvec2")
    return 8;
  if(type == "vec3" || type == "ivec3" || type == "uvec3")
    return 12;
  if(type == "vec4" || type == "ivec4" || type == "uvec4")
    return 16;
  if(type == "mat4")
    return 64;
  return 16; // fallback
}

// Returns the byte size of an ossia::geometry attribute format
static int geometryFormatSizeBytes(int format) noexcept
{
  using F = ossia::geometry::attribute;
  switch(format)
  {
    case F::float4: return 16;
    case F::float3: return 12;
    case F::float2: return 8;
    case F::float1: return 4;
    case F::unormbyte4: return 4;
    case F::unormbyte2: return 2;
    case F::unormbyte1: return 1;
    case F::uint4: return 16;
    case F::uint3: return 12;
    case F::uint2: return 8;
    case F::uint1: return 4;
    case F::sint4: return 16;
    case F::sint3: return 12;
    case F::sint2: return 8;
    case F::sint1: return 4;
    case F::half4: return 8;
    case F::half3: return 6;
    case F::half2: return 4;
    case F::half1: return 2;
    default: return 4;
  }
}

void RenderedCSFNode::updateGeometryBindings(
    RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  if(!this->geometry.meshes || this->geometry.meshes->meshes.empty())
    return;

  const auto& mesh = this->geometry.meshes->meshes[0]; // FIXME multi-mesh
  int geo_binding_idx = 0;

  for(const auto& input : n.m_descriptor.inputs)
  {
    auto* geo_input = ossia::get_if<isf::geometry_input>(&input.data);
    if(!geo_input)
      continue;

    if(geo_binding_idx >= (int)m_geometryBindings.size())
      break;

    auto& binding = m_geometryBindings[geo_binding_idx];
    binding.element_count = mesh.vertices;

    // For each attribute the shader declared interest in
    for(int attr_idx = 0; attr_idx < (int)geo_input->attributes.size(); attr_idx++)
    {
      if(attr_idx >= (int)binding.attribute_ssbos.size())
        break;

      const auto& req = geo_input->attributes[attr_idx];
      auto& ssbo = binding.attribute_ssbos[attr_idx];

      // Match by semantic
      const ossia::attribute_semantic sem = ossia::name_to_semantic(req.semantic);
      const ossia::geometry::attribute* geo_attr = nullptr;
      if(sem != ossia::attribute_semantic::custom)
        geo_attr = mesh.find(sem);
      else
        geo_attr = mesh.find(req.name);

      if(!geo_attr)
      {
        if(req.required)
          qWarning() << "CSF geometry: required attribute" << req.name.c_str() << "not found";

        // Create or keep a zero-filled fallback buffer
        const int elem_size = glslTypeSizeBytes(req.type);
        const int64_t needed = (int64_t)elem_size * std::max(1, mesh.vertices);
        if(!ssbo.buffer || ssbo.size < needed)
        {
          if(ssbo.buffer && ssbo.owned)
          {
            ssbo.buffer->destroy();
            delete ssbo.buffer;
          }
          auto* buf = renderer.state.rhi->newBuffer(
              QRhiBuffer::Static,
              QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer, needed);
          buf->setName(QByteArray("CSF_GeomFallback_") + req.name.c_str());
          buf->create();
          QByteArray zero(needed, 0);
          res.uploadStaticBuffer(buf, 0, needed, zero.constData());
          ssbo.buffer = buf;
          ssbo.size = needed;
          ssbo.owned = true;
        }
        continue;
      }

      // Found the attribute — extract its buffer data
      const int buf_idx = geo_attr->binding;
      if(buf_idx < 0 || buf_idx >= (int)mesh.buffers.size())
        continue;

      const auto& geo_buf = mesh.buffers[buf_idx];
      const auto& geo_bind = (buf_idx < (int)mesh.bindings.size())
                                 ? mesh.bindings[buf_idx]
                                 : mesh.bindings[0];

      const int attr_size = geometryFormatSizeBytes(geo_attr->format);
      const int stride = geo_bind.byte_stride;
      const bool is_soa = (stride == 0 || stride == attr_size);

      if(auto* gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&geo_buf.data))
      {
        if(is_soa && gpu->handle)
        {
          // SoA GPU buffer: bind directly (zero-copy)
          if(ssbo.owned && ssbo.buffer)
          {
            ssbo.buffer->destroy();
            delete ssbo.buffer;
          }
          ssbo.buffer = static_cast<QRhiBuffer*>(gpu->handle);
          ssbo.size = gpu->byte_size;
          ssbo.owned = false;
          continue;
        }
        // AoS GPU buffer: would need scatter compute pass — not yet supported
        qWarning() << "CSF geometry: AoS GPU buffer scatter not yet implemented for"
                    << req.name.c_str();
        continue;
      }

      if(auto* cpu = ossia::get_if<ossia::geometry::cpu_buffer>(&geo_buf.data))
      {
        if(!cpu->raw_data || cpu->byte_size <= 0)
          continue;

        const auto* src = static_cast<const char*>(cpu->raw_data.get());
        const int64_t elem_size = glslTypeSizeBytes(req.type);
        const int64_t needed = elem_size * mesh.vertices;

        // Create or resize the SSBO
        if(!ssbo.buffer || ssbo.size < needed || !ssbo.owned)
        {
          if(ssbo.owned && ssbo.buffer)
          {
            ssbo.buffer->destroy();
            delete ssbo.buffer;
          }
          auto* buf = renderer.state.rhi->newBuffer(
              QRhiBuffer::Static,
              QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer, needed);
          buf->setName(QByteArray("CSF_Geom_") + req.name.c_str());
          buf->create();
          ssbo.buffer = buf;
          ssbo.size = needed;
          ssbo.owned = true;
        }

        if(is_soa)
        {
          // SoA CPU buffer: upload directly
          const int64_t upload_size = std::min(needed, cpu->byte_size);
          res.uploadStaticBuffer(ssbo.buffer, 0, upload_size, src + geo_attr->byte_offset);
        }
        else
        {
          // AoS CPU buffer: scatter attribute data into flat SoA buffer
          QByteArray scattered(needed, 0);
          const int copy_size = std::min((int)elem_size, attr_size);
          for(int i = 0; i < mesh.vertices; i++)
          {
            const int64_t src_off = (int64_t)i * stride + geo_attr->byte_offset;
            if(src_off + copy_size <= cpu->byte_size)
              std::memcpy(scattered.data() + (int64_t)i * elem_size, src + src_off, copy_size);
          }
          res.uploadStaticBuffer(ssbo.buffer, 0, needed, scattered.constData());
        }
      }
    }

    geo_binding_idx++;
  }
}

void RenderedCSFNode::pushOutputGeometry(RenderList& renderer, Edge& edge)
{
  // For each geometry_input with writable attributes, construct output geometry
  // and push to downstream node's renderer
  int geo_binding_idx = 0;
  int geo_output_idx = 0;

  for(const auto& input : n.m_descriptor.inputs)
  {
    auto* geo_input = ossia::get_if<isf::geometry_input>(&input.data);
    if(!geo_input)
      continue;

    if(geo_binding_idx >= (int)m_geometryBindings.size())
      break;

    auto& binding = m_geometryBindings[geo_binding_idx];

    if(!binding.has_output)
    {
      geo_binding_idx++;
      continue;
    }

    // Build output geometry_spec with the modified SSBO handles
    auto meshes = std::make_shared<ossia::mesh_list>();
    ossia::geometry out_geo;
    out_geo.vertices = binding.element_count;
    out_geo.topology = ossia::geometry::points;
    out_geo.cull_mode = ossia::geometry::none;
    out_geo.front_face = ossia::geometry::counter_clockwise;

    // Copy bounds from input geometry if available
    if(this->geometry.meshes && !this->geometry.meshes->meshes.empty())
      out_geo.bounds = this->geometry.meshes->meshes[0].bounds;

    for(int attr_idx = 0; attr_idx < (int)geo_input->attributes.size(); attr_idx++)
    {
      if(attr_idx >= (int)binding.attribute_ssbos.size())
        break;

      const auto& req = geo_input->attributes[attr_idx];
      auto& ssbo = binding.attribute_ssbos[attr_idx];

      if(!ssbo.buffer)
        continue;

      // Each attribute gets its own buffer (SoA)
      const int buf_index = (int)out_geo.buffers.size();
      const int elem_size = glslTypeSizeBytes(req.type);

      ossia::geometry::buffer buf{
          .data = ossia::geometry::gpu_buffer{ssbo.buffer, ssbo.size},
          .dirty = false};
      out_geo.buffers.push_back(std::move(buf));

      ossia::geometry::binding bind;
      bind.byte_stride = elem_size;
      bind.classification = ossia::geometry::binding::per_vertex;
      out_geo.bindings.push_back(bind);

      ossia::geometry::attribute attr;
      attr.binding = buf_index;
      attr.location = attr_idx;
      const auto sem = ossia::name_to_semantic(req.semantic);
      attr.semantic = sem;
      if(sem == ossia::attribute_semantic::custom)
        attr.name = req.name;

      // Map GLSL type to format enum
      if(req.type == "vec4") attr.format = ossia::geometry::attribute::float4;
      else if(req.type == "vec3") attr.format = ossia::geometry::attribute::float3;
      else if(req.type == "vec2") attr.format = ossia::geometry::attribute::float2;
      else if(req.type == "float") attr.format = ossia::geometry::attribute::float1;
      else if(req.type == "ivec4" || req.type == "uvec4") attr.format = ossia::geometry::attribute::uint4;
      else if(req.type == "ivec3" || req.type == "uvec3") attr.format = ossia::geometry::attribute::uint3;
      else if(req.type == "ivec2" || req.type == "uvec2") attr.format = ossia::geometry::attribute::uint2;
      else if(req.type == "int" || req.type == "uint") attr.format = ossia::geometry::attribute::uint1;
      else attr.format = ossia::geometry::attribute::float4;

      attr.byte_offset = 0;
      out_geo.attributes.push_back(attr);

      struct ossia::geometry::input geo_inp;
      geo_inp.buffer = buf_index;
      geo_inp.byte_offset = 0;
      out_geo.input.push_back(geo_inp);
    }

    meshes->meshes.push_back(std::move(out_geo));

    m_outputGeometry.meshes = meshes;
    m_outputGeometry.filters = {};

    // Find the geometry output port and push to downstream
    // The output ports are ordered: first image output, then storage outputs,
    // then geometry outputs
    const auto& outlets = n.output;
    // Walk descriptor inputs to count outlet index for this geometry output
    int outlet_idx = 0;
    for(const auto& inp : n.m_descriptor.inputs)
    {
      if(auto* s = ossia::get_if<isf::storage_input>(&inp.data))
      {
        if(s->access != "read_only")
          outlet_idx++;
      }
      else if(auto* img = ossia::get_if<isf::csf_image_input>(&inp.data))
      {
        if(img->access != "read_only")
          outlet_idx++;
      }
      else if(ossia::get_if<isf::geometry_input>(&inp.data))
      {
        break; // this is our geometry_input
      }
    }
    outlet_idx += geo_output_idx;

    // Check if default image output exists (always first)
    // Usually outlet[0] is the image output, geometry outputs come after
    if(!outlets.empty() && outlets[0]->type == Types::Image)
      outlet_idx++; // skip the default image output

    if(outlet_idx < (int)outlets.size())
    {
      auto* out_port = outlets[outlet_idx];
      for(auto* out_edge : out_port->edges)
      {
        auto* sink = out_edge->sink;
        if(!sink || !sink->node)
          continue;

        auto rendered = sink->node->renderedNodes.find(&renderer);
        if(rendered == sink->node->renderedNodes.end())
          continue;

        auto it = std::find(
            sink->node->input.begin(), sink->node->input.end(), sink);
        if(it == sink->node->input.end())
          continue;

        int port_idx = it - sink->node->input.begin();
        rendered->second->process(port_idx, m_outputGeometry);
      }
    }

    geo_binding_idx++;
    geo_output_idx++;
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
  int geo_binding_index = 0;
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
    // Geometry inputs: bind per-attribute SSBOs
    else if(auto* geo_input = ossia::get_if<isf::geometry_input>(&input.data))
    {
      // Update geometry bindings from incoming geometry data
      updateGeometryBindings(renderer, res);

      if(geo_binding_index < (int)m_geometryBindings.size())
      {
        auto& binding = m_geometryBindings[geo_binding_index];

        for(int attr_idx = 0; attr_idx < (int)geo_input->attributes.size(); attr_idx++)
        {
          if(attr_idx >= (int)binding.attribute_ssbos.size())
            break;

          const auto& req = geo_input->attributes[attr_idx];
          auto& ssbo = binding.attribute_ssbos[attr_idx];

          if(!ssbo.buffer)
          {
            // Create a minimal fallback buffer so we don't crash
            const int elem_size = glslTypeSizeBytes(req.type);
            ssbo.buffer = rhi.newBuffer(
                QRhiBuffer::Static,
                QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer, elem_size);
            ssbo.buffer->setName(QByteArray("CSF_GeomInit_") + req.name.c_str());
            ssbo.buffer->create();
            ssbo.size = elem_size;
            ssbo.owned = true;
          }

          if(req.access == "read_only")
          {
            bindings.append(QRhiShaderResourceBinding::bufferLoad(
                bindingIndex++, QRhiShaderResourceBinding::ComputeStage, ssbo.buffer));
          }
          else if(req.access == "write_only")
          {
            bindings.append(QRhiShaderResourceBinding::bufferStore(
                bindingIndex++, QRhiShaderResourceBinding::ComputeStage, ssbo.buffer));
          }
          else // read_write
          {
            bindings.append(QRhiShaderResourceBinding::bufferLoadStore(
                bindingIndex++, QRhiShaderResourceBinding::ComputeStage, ssbo.buffer));
          }
        }
        geo_binding_index++;
      }
      input_port_index++;
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
    // Handle geometry inputs
    else if(auto* geo = ossia::get_if<isf::geometry_input>(&input.data))
    {
      GeometryBinding binding;
      binding.has_output = false;

      for(const auto& attr : geo->attributes)
      {
        GeometryBinding::AttributeSSBO ssbo;
        ssbo.name = attr.name;
        ssbo.access = attr.access;
        binding.attribute_ssbos.push_back(std::move(ssbo));

        if(attr.access != "read_only")
          binding.has_output = true;
      }

      const bool geo_has_output = binding.has_output;
      m_geometryBindings.push_back(std::move(binding));

      if(geo_has_output)
        outlet_index++;
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

  // Update geometry bindings if geometry data changed
  if(this->geometryChanged && !m_geometryBindings.empty())
  {
    updateGeometryBindings(renderer, res);
    this->geometryChanged = false;
  }

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
  int geo_binding_index = 0;

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
    // Geometry inputs: rebind per-attribute SSBOs
    else if(auto* geo_input = ossia::get_if<isf::geometry_input>(&input.data))
    {
      if(geo_binding_index < (int)m_geometryBindings.size())
      {
        auto& binding = m_geometryBindings[geo_binding_index];

        for(int attr_idx = 0; attr_idx < (int)geo_input->attributes.size(); attr_idx++)
        {
          if(attr_idx >= (int)binding.attribute_ssbos.size())
            break;

          const auto& req = geo_input->attributes[attr_idx];
          auto& ssbo = binding.attribute_ssbos[attr_idx];

          if(!ssbo.buffer)
            continue;

          if(req.access == "read_only")
          {
            bindings.append(QRhiShaderResourceBinding::bufferLoad(
                bindingIndex++, QRhiShaderResourceBinding::ComputeStage, ssbo.buffer));
          }
          else if(req.access == "write_only")
          {
            bindings.append(QRhiShaderResourceBinding::bufferStore(
                bindingIndex++, QRhiShaderResourceBinding::ComputeStage, ssbo.buffer));
          }
          else // read_write
          {
            bindings.append(QRhiShaderResourceBinding::bufferLoadStore(
                bindingIndex++, QRhiShaderResourceBinding::ComputeStage, ssbo.buffer));
          }
        }
        geo_binding_index++;
      }
      input_port_index++;
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
      pass.srb->destroy();
    }
    else
    {
      // Create new SRB
      pass.srb = rhi.newShaderResourceBindings();
    }

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

  // Clean up geometry bindings
  for(auto& binding : m_geometryBindings)
  {
    for(auto& ssbo : binding.attribute_ssbos)
    {
      if(ssbo.owned && ssbo.buffer)
      {
        r.releaseBuffer(ssbo.buffer);
      }
      ssbo.buffer = nullptr;
    }
  }
  m_geometryBindings.clear();
  
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

      // If no storage buffer matched, try geometry element count
      if(n <= 1)
      {
        for(const auto& geo_bind : m_geometryBindings)
        {
          if(geo_bind.element_count > 0)
          {
            n = geo_bind.element_count;
            break;
          }
        }
      }

      const auto requiredInvocations = n;
      const auto threadsPerWorkgroup = localX * localY * localZ;
      const int64_t totalWorkgroups = (requiredInvocations + threadsPerWorkgroup - 1)
                                      / (threadsPerWorkgroup * passDesc.stride);
      static constexpr int64_t maxWorkgroups = 65535;

      if(totalWorkgroups > maxWorkgroups * maxWorkgroups * maxWorkgroups)
      {
        return;
      }
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
      else if(totalWorkgroups > 0)
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

  // After all compute passes: push output geometry to downstream nodes
  if(!m_geometryBindings.empty())
  {
    pushOutputGeometry(renderer, edge);
  }
}
}
