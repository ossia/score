#include "ossia/detail/fmt.hpp"

#include <Gfx/Graph/CommonUBOs.hpp>
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/RenderedCSFNode.hpp>
#include <Gfx/Graph/RenderedISFSamplerUtils.hpp>
#include <Gfx/Graph/RhiComputeBarrier.hpp>
#include <Gfx/Graph/SSBO.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/dataflow/geometry_port.hpp>
#include <ossia/detail/algorithms.hpp>
#include <ossia/math/math_expression.hpp>

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

QRhiTexture* RenderedCSFNode::textureForOutput(const Port& output)
{
  // Look up the specific texture for this output port
  for(auto& [port, index] : m_outStorageImages)
  {
    if(&output == port)
    {
      if(index < (int)m_storageImages.size())
        return m_storageImages[index].texture;
    }
  }

  // Fallback: return the first output texture (for default output port)
  if(output.type == Types::Image && m_outputTexture)
    return m_outputTexture;
  return nullptr;
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
    if(v.attributes.empty())
    {
      // Pure pass-through: one inlet + one outlet
      inlet_i++;
      outlet_i++;
    }
    else
    {
      // Inlet if any attribute needs upstream data (read_only or read_write)
      for(const auto& attr : v.attributes)
        if(attr.access == "read_only" || attr.access == "read_write") { inlet_i++; break; }
      // Outlet if any attribute is writable (write_only or read_write)
      for(const auto& attr : v.attributes)
      {
        if(attr.access == "write_only" || attr.access == "read_write")
        {
          outlet_i++; // one geometry output port if any attribute is writable
          break;
        }
      }
    }
    // $USER ports for vertex_count, instance_count, aux.size
    if(v.vertex_count.find("$USER") != std::string::npos) inlet_i++;
    if(v.instance_count.find("$USER") != std::string::npos) inlet_i++;
    for(const auto& aux : v.auxiliary)
      if(aux.size.find("$USER") != std::string::npos) inlet_i++;
  }
  void operator()(const auto& v) { inlet_i++; }
};
QSize RenderedCSFNode::computeTextureSize(
    const isf::csf_image_input& pass) const noexcept
{
  ossia::math_expression e;
  ossia::small_pod_vector<double, 16> data;

  // Note : reserve is super important here,
  // as the expression parser takes *references* to the variables.
  data.reserve(2 + 2 * m_inputSamplers.size() + n.descriptor().inputs.size() + 2 * m_geometryBindings.size());

  registerCommonExpressionVariables(e, data);

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

int RenderedCSFNode::resolveCountExpression(
    const std::string& expr, const isf::geometry_input& geo,
    const std::string& fieldName) const
{
  if(expr.empty())
    return 0;

  // Try fixed integer first
  try
  {
    return std::max(1, std::stoi(expr));
  }
  catch(...)
  {
  }

  // Build expression evaluator
  ossia::math_expression e;
  ossia::small_pod_vector<double, 16> data;

  const auto& desc = n.descriptor();
  data.reserve(2 + 2 * m_inputSamplers.size() + desc.inputs.size() + 2 * m_geometryBindings.size() + 1);

  registerCommonExpressionVariables(e, data);

  // Find the $USER port for this specific geometry field
  port_indices p;
  int user_port_index = -1;
  for(const auto& input : desc.inputs)
  {
    auto* geo_inp = ossia::get_if<isf::geometry_input>(&input.data);

    // For the matching geometry_input, find the $USER port index
    if(geo_inp && geo_inp == &geo)
    {
      // Skip past geometry inlet port (exists if any attr reads from upstream)
      int cur = p.inlet_i;
      for(const auto& attr : geo.attributes)
        if(attr.access == "read_only" || attr.access == "read_write") { cur++; break; }

      if(fieldName == "vertex_count" && geo.vertex_count.find("$USER") != std::string::npos)
      {
        user_port_index = cur;
      }
      cur += (geo.vertex_count.find("$USER") != std::string::npos) ? 1 : 0;

      if(fieldName == "instance_count" && geo.instance_count.find("$USER") != std::string::npos)
      {
        user_port_index = cur;
      }
      cur += (geo.instance_count.find("$USER") != std::string::npos) ? 1 : 0;

      for(const auto& aux : geo.auxiliary)
      {
        if(aux.size.find("$USER") != std::string::npos)
        {
          if(fieldName == aux.name)
            user_port_index = cur;
          cur++;
        }
      }
      break;
    }

    ossia::visit(p, input.data);
  }

  // Register $USER value
  if(user_port_index >= 0 && user_port_index < (int)n.input.size())
  {
    auto port = n.input[user_port_index];
    if(port && port->value)
      e.add_constant("var_USER", data.emplace_back(*(int*)port->value));
    else
      e.add_constant("var_USER", data.emplace_back(1));
  }

  // Evaluate expression
  auto eval_expr = expr;
  boost::algorithm::replace_all(eval_expr, "$", "var_");
  e.register_symbol_table();
  bool ok = e.set_expression(eval_expr);
  if(ok)
    return std::max(1, (int)e.value());

  qDebug() << "resolveCountExpression failed:" << e.error().c_str() << eval_expr.c_str();
  return 0;
}

void RenderedCSFNode::registerCommonExpressionVariables(
    ossia::math_expression& e, ossia::small_pod_vector<double, 16>& data) const
{
  const auto& desc = n.descriptor();

  // Register texture dimensions ($WIDTH_<name>, $HEIGHT_<name>)
  int input_image_index = 0;
  for(const auto& img : desc.inputs)
  {
    if(ossia::get_if<isf::texture_input>(&img.data))
    {
      if(input_image_index < (int)m_inputSamplers.size())
      {
        auto [s, t] = this->m_inputSamplers[input_image_index];
        QSize tex_sz = t ? t->pixelSize() : QSize{1280, 720};
        e.add_constant(
            fmt::format("var_WIDTH_{}", img.name), data.emplace_back(tex_sz.width()));
        e.add_constant(
            fmt::format("var_HEIGHT_{}", img.name), data.emplace_back(tex_sz.height()));
      }
      input_image_index++;
    }
    else if(auto* img_input = ossia::get_if<isf::csf_image_input>(&img.data))
    {
      if(img_input->access == "read_only")
      {
        if(input_image_index < (int)m_inputSamplers.size())
        {
          auto [s, t] = this->m_inputSamplers[input_image_index];
          QSize tex_sz = t ? t->pixelSize() : QSize{1280, 720};
          e.add_constant(
              fmt::format("var_WIDTH_{}", img.name), data.emplace_back(tex_sz.width()));
          e.add_constant(
              fmt::format("var_HEIGHT_{}", img.name), data.emplace_back(tex_sz.height()));
        }
        input_image_index++;
      }
    }
  }

  // Register scalar inputs ($<inputName>)
  port_indices p;
  for(const auto& input : desc.inputs)
  {
    if(ossia::get_if<isf::float_input>(&input.data))
    {
      auto port = n.input[p.inlet_i];
      if(port && port->value)
        e.add_constant("var_" + input.name, data.emplace_back(*(float*)port->value));
    }
    else if(ossia::get_if<isf::long_input>(&input.data))
    {
      auto port = n.input[p.inlet_i];
      if(port && port->value)
        e.add_constant("var_" + input.name, data.emplace_back(*(int*)port->value));
    }

    ossia::visit(p, input.data);
  }

  // Register named geometry vertex/instance counts
  // ($VERTEX_COUNT_<name>, $INSTANCE_COUNT_<name>, and first one as $VERTEX_COUNT, $INSTANCE_COUNT)
  int geo_idx = 0;
  bool first_geo = true;
  for(const auto& input : desc.inputs)
  {
    if(ossia::get_if<isf::geometry_input>(&input.data))
    {
      if(geo_idx < (int)m_geometryBindings.size())
      {
        const auto& geo_bind = m_geometryBindings[geo_idx];
        if(geo_bind.vertex_count > 0)
        {
          e.add_constant(
              fmt::format("var_VERTEX_COUNT_{}", input.name),
              data.emplace_back(geo_bind.vertex_count));
          if(first_geo)
            e.add_constant("var_VERTEX_COUNT", data.emplace_back(geo_bind.vertex_count));
        }
        if(geo_bind.instance_count > 0)
        {
          e.add_constant(
              fmt::format("var_INSTANCE_COUNT_{}", input.name),
              data.emplace_back(geo_bind.instance_count));
          if(first_geo)
            e.add_constant("var_INSTANCE_COUNT", data.emplace_back(geo_bind.instance_count));
        }
        first_geo = false;
      }
      geo_idx++;
    }
  }
}

int RenderedCSFNode::resolveDispatchExpression(const std::string& expr) const
{
  if(expr.empty())
    return 1;

  // Try fixed integer first
  try
  {
    return std::max(1, std::stoi(expr));
  }
  catch(...)
  {
  }

  // Build expression evaluator
  ossia::math_expression e;
  ossia::small_pod_vector<double, 16> data;
  data.reserve(2 + 2 * m_inputSamplers.size() + n.descriptor().inputs.size() + 2 * m_geometryBindings.size());

  registerCommonExpressionVariables(e, data);

  // Evaluate expression
  auto eval_expr = expr;
  boost::algorithm::replace_all(eval_expr, "$", "var_");
  e.register_symbol_table();
  bool ok = e.set_expression(eval_expr);
  if(ok)
    return std::max(1, (int)e.value());

  qDebug() << "resolveDispatchExpression failed:" << e.error().c_str() << eval_expr.c_str();
  return 1;
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
  qWarning() << "CSF ALLOC [createStorageBuffer]" << name << "size=" << size;

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
      auto& sb = this->m_storageBuffers[index];
      BufferView bv{sb.buffer, 0, sb.buffer ? sb.buffer->size() : 0};
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
      if(sb.buffer_usage == "indirect_draw")
        bv.usage = BufferView::Usage::IndirectDraw;
      else if(sb.buffer_usage == "indirect_draw_indexed")
        bv.usage = BufferView::Usage::IndirectDrawIndexed;
#endif
      return bv;
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
    // Check if any incoming geometry port has a matching auxiliary buffer.
    // If so, use that GPU buffer directly instead of creating our own.
    // Search all port geometries since storage buffers aren't tied to a specific port.
    const auto stdName = storageBuffer.name.toStdString();
    bool found_aux = false;
    for(const auto& [port_idx, geo_spec] : m_portGeometries)
    {
      if(!geo_spec.meshes || geo_spec.meshes->meshes.empty())
        continue;
      const auto& mesh = geo_spec.meshes->meshes[0];
      if(auto* aux = mesh.find_auxiliary(stdName))
      {
        if(aux->buffer >= 0 && aux->buffer < (int)mesh.buffers.size())
        {
          const auto& geo_buf = mesh.buffers[aux->buffer];
          if(auto* gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&geo_buf.data))
          {
            if(gpu->handle)
            {
              auto* rhi_buf = static_cast<QRhiBuffer*>(gpu->handle);
              if(storageBuffer.buffer != rhi_buf)
              {
                // Release our owned buffer if we had one
                if(storageBuffer.owned && storageBuffer.buffer)
                {
                  renderer.releaseBuffer(storageBuffer.buffer);
                }
                storageBuffer.buffer = rhi_buf;
                storageBuffer.size = aux->byte_size > 0 ? aux->byte_size : gpu->byte_size;
                storageBuffer.lastKnownSize = storageBuffer.size;
                storageBuffer.owned = false;
                buffersChanged = true;
              }
              found_aux = true;
              break;
            }
          }
        }
      }
      if(found_aux)
        break;
    }
    if(found_aux)
      continue;

    // No auxiliary buffer match — manage our own buffer
    if(!storageBuffer.owned && storageBuffer.buffer)
    {
      // Was using an auxiliary buffer that's no longer available;
      // need to create our own
      storageBuffer.buffer = nullptr;
      storageBuffer.owned = true;
    }

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
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
        if(!storageBuffer.buffer_usage.empty()
           && (storageBuffer.buffer_usage == "indirect_draw"
               || storageBuffer.buffer_usage == "indirect_draw_indexed"))
        {
          QRhi& rhi = *renderer.state.rhi;
          storageBuffer.buffer = rhi.newBuffer(
              QRhiBuffer::Static,
              QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer
                  | QRhiBuffer::IndirectBuffer,
              requiredSize);
          qWarning() << "CSF ALLOC [updateStorage/indirect]" << storageBuffer.name << "size=" << requiredSize;
          if(storageBuffer.buffer)
          {
            storageBuffer.buffer->setName(
                QStringLiteral("CSF_IndirectStorageBuffer_%1")
                    .arg(storageBuffer.name)
                    .toLocal8Bit());
            if(!storageBuffer.buffer->create())
            {
              delete storageBuffer.buffer;
              storageBuffer.buffer = nullptr;
            }
          }
        }
        else
#endif
        {
          storageBuffer.buffer
              = createStorageBuffer(
                    renderer, storageBuffer.name, storageBuffer.access, requiredSize)
                    .handle;
        }
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

  // SRBs will be recreated once at the end of update(), after all buffer
  // mutations (storage + geometry) are finalized. This prevents building
  // intermediate SRBs that reference stale/dangling buffer pointers.
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
  // Pre-pass: populate vertex/instance counts from upstream for ALL bindings first,
  // so that expression resolution (e.g. $VERTEX_COUNT_geoIn) can reference any binding.
  {
    int pre_idx = 0;
    for(const auto& input : n.m_descriptor.inputs)
    {
      auto* geo_input = ossia::get_if<isf::geometry_input>(&input.data);
      if(!geo_input)
        continue;
      if(pre_idx >= (int)m_geometryBindings.size())
        break;
      auto& binding = m_geometryBindings[pre_idx];
      if(binding.input_port_index >= 0 && !binding.has_vertex_count_spec)
      {
        auto it = m_portGeometries.find(binding.input_port_index);
        if(it != m_portGeometries.end()
           && it->second.meshes && !it->second.meshes->meshes.empty())
        {
          binding.vertex_count = it->second.meshes->meshes[0].vertices;
          if(it->second.meshes->meshes[0].instances > 0)
            binding.instance_count = it->second.meshes->meshes[0].instances;
        }
      }
      pre_idx++;
    }
  }

  int geo_binding_idx = 0;

  for(const auto& input : n.m_descriptor.inputs)
  {
    auto* geo_input = ossia::get_if<isf::geometry_input>(&input.data);
    if(!geo_input)
      continue;

    if(geo_binding_idx >= (int)m_geometryBindings.size())
      break;

    auto& binding = m_geometryBindings[geo_binding_idx];

    // Per-binding upstream lookup: use this binding's port index
    bool binding_has_upstream = false;
    const ossia::geometry* upstream_mesh = nullptr;
    if(binding.input_port_index >= 0)
    {
      auto it = m_portGeometries.find(binding.input_port_index);
      if(it != m_portGeometries.end()
         && it->second.meshes && !it->second.meshes->meshes.empty())
      {
        binding_has_upstream = true;
        upstream_mesh = &it->second.meshes->meshes[0];
      }
    }


    // Resolve vertex_count expression if specified
    if(binding.has_vertex_count_spec)
    {
      int count = resolveCountExpression(geo_input->vertex_count, *geo_input, "vertex_count");
      if(count > 0)
        binding.vertex_count = count;
    }

    // Resolve instance_count expression if specified
    if(binding.has_instance_count_spec)
    {
      int ic = resolveCountExpression(geo_input->instance_count, *geo_input, "instance_count");
      if(ic > 0)
        binding.instance_count = ic;
    }

    // Resolve auxiliary size expressions and resize those buffers
    for(int aux_idx = 0; aux_idx < (int)binding.auxiliary_ssbos.size(); aux_idx++)
    {
      auto& aux = binding.auxiliary_ssbos[aux_idx];
      if(aux.size_expr.empty())
        continue;

      int arrayCount = resolveCountExpression(aux.size_expr, *geo_input, aux.name);
      if(arrayCount <= 0)
        continue;

      const int64_t requiredSize = score::gfx::calculateStorageBufferSize(
          aux.layout, arrayCount, this->n.descriptor());
      if(requiredSize > 0 && requiredSize != aux.size)
      {
        if(aux.buffer && aux.owned)
        {
          aux.buffer->destroy();
          aux.buffer->setSize(requiredSize);
          aux.buffer->create();
        }
        else
        {
          auto* buf = renderer.state.rhi->newBuffer(
              QRhiBuffer::Static,
              QRhiBuffer::StorageBuffer, requiredSize);
          qWarning() << "CSF ALLOC [auxResize]" << aux.name.c_str() << "size=" << requiredSize;
          buf->setName(QByteArray("CSF_GeoAux_") + aux.name.c_str());
          buf->create();
          aux.buffer = buf;
          aux.owned = true;
        }
        QByteArray zero(requiredSize, 0);
        res.uploadStaticBuffer(aux.buffer, 0, requiredSize, zero.constData());
        aux.size = requiredSize;
      }
    }

    // Detect feedback receiver: a GENERATOR (has vertex_count_spec) that receives
    // upstream geometry ON ITS OWN PORT. Only then is it a feedback loop.
    // Bindings whose port receives external data (from a different node) should
    // NOT be marked as feedback receivers.
    if(binding.has_vertex_count_spec && binding_has_upstream && !binding.is_feedback_receiver)
    {
      // Heuristic: check if the upstream buffer pointers match our own SSBOs.
      // If they do, this is genuine feedback (our own output looped back).
      // If they don't, the upstream is from a different node.
      bool is_self_feedback = false;
      if(upstream_mesh)
      {
        for(const auto& up_attr : upstream_mesh->attributes)
        {
          if(up_attr.binding < 0 || up_attr.binding >= (int)upstream_mesh->buffers.size())
            continue;
          const auto& up_buf = upstream_mesh->buffers[up_attr.binding];
          if(auto* gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&up_buf.data))
          {
            if(gpu->handle)
            {
              // Check if this GPU handle matches one of our own SSBOs
              for(const auto& ssbo : binding.attribute_ssbos)
              {
                if(ssbo.buffer == static_cast<QRhiBuffer*>(gpu->handle))
                {
                  is_self_feedback = true;
                  break;
                }
              }
              if(is_self_feedback)
                break;
            }
          }
        }
      }

      if(is_self_feedback)
      {
        binding.is_feedback_receiver = true;
        binding.pending_initial_copy = true;
        for(int attr_idx = 0; attr_idx < (int)geo_input->attributes.size(); attr_idx++)
        {
          if(attr_idx >= (int)binding.attribute_ssbos.size())
            break;
          const auto& req = geo_input->attributes[attr_idx];
          auto& ssbo = binding.attribute_ssbos[attr_idx];
          if(req.access == "read_write" && !ssbo.read_buffer)
          {
            const int elem_size = glslTypeSizeBytes(req.type);
            const int count = ssbo.per_instance ? binding.instance_count : binding.vertex_count;
            const int64_t buf_size = (int64_t)elem_size * count;
            if(buf_size > 0)
            {
              auto* buf = renderer.state.rhi->newBuffer(
                  QRhiBuffer::Static,
                  QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer, buf_size);
              qWarning() << "CSF ALLOC [feedbackPingPong]" << req.name.c_str() << "size=" << buf_size;
              buf->setName(QByteArray("CSF_GeomPP_") + req.name.c_str());
              buf->create();
              QByteArray zero(buf_size, 0);
              res.uploadStaticBuffer(buf, 0, buf_size, zero.constData());
              ssbo.read_buffer = buf;
            }
          }
        }
      }
    }

    if(!binding_has_upstream && !binding.has_vertex_count_spec)
    {
      // No upstream geometry on this binding's port and no vertex_count spec.
      // Clear any stale unowned pointers.
      for(auto& ssbo : binding.attribute_ssbos)
      {
        if(!ssbo.owned)
        {
          ssbo.buffer = nullptr;
          ssbo.owned = true;
        }
      }
      for(auto& aux : binding.auxiliary_ssbos)
      {
        if(!aux.owned)
        {
          aux.buffer = nullptr;
          aux.owned = true;
        }
      }
      geo_binding_idx++;
      continue;
    }

    if(binding_has_upstream && upstream_mesh)
    {
      const auto& mesh = *upstream_mesh;
      if(!binding.has_vertex_count_spec)
        binding.vertex_count = mesh.vertices;

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
          // Create or keep a zero-filled fallback buffer
          const int elem_size = glslTypeSizeBytes(req.type);
          const int fallback_count = ssbo.per_instance ? std::max(1, mesh.instances) : std::max(1, mesh.vertices);
          const int64_t needed = (int64_t)elem_size * fallback_count;
          if(!ssbo.buffer || ssbo.size < needed)
          {
            if(req.required && req.access == "read_only")
              qWarning() << "CSF geometry: required read_only attribute" << req.name.c_str() << "not found"
                         << "(semantic=" << (int)sem << ")";
            else
              qDebug() << "  attr" << req.name.c_str() << "not in upstream — creating fallback buffer";

            if(ssbo.buffer && ssbo.owned)
            {
              renderer.releaseBuffer(ssbo.buffer);
            }
            auto* buf = renderer.state.rhi->newBuffer(
                QRhiBuffer::Static,
                QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer, needed);
            qWarning() << "CSF ALLOC [geomFallback]" << req.name.c_str() << "size=" << needed;
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

        // Found the attribute — extract its buffer data.
        // The attribute's binding index maps to a binding (stride/classification)
        // and an input entry (which buffer + byte offset within that buffer).
        const int binding_idx = geo_attr->binding;
        if(binding_idx < 0 || binding_idx >= (int)mesh.input.size())
          continue;

        const auto& geo_inp = mesh.input[binding_idx];
        const int buf_idx = geo_inp.buffer;
        if(buf_idx < 0 || buf_idx >= (int)mesh.buffers.size())
          continue;

        const int64_t input_byte_offset = geo_inp.byte_offset;
        const auto& geo_buf = mesh.buffers[buf_idx];
        const auto& geo_bind = (binding_idx < (int)mesh.bindings.size())
                                   ? mesh.bindings[binding_idx]
                                   : mesh.bindings[0];

        const int attr_size = geometryFormatSizeBytes(geo_attr->format);
        const int stride = geo_bind.byte_stride;
        const bool is_soa = (stride == 0 || stride == attr_size);

        if(auto* gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&geo_buf.data))
        {
          const int elem_size = glslTypeSizeBytes(req.type);
          if(is_soa && gpu->handle && attr_size == elem_size)
          {
            // SoA GPU buffer with matching element size: bind directly (zero-copy)
            auto* rhi_buf = static_cast<QRhiBuffer*>(gpu->handle);

            // If this node has a vertex_count_spec ($USER), its own SSBOs are
            // authoritative. Don't replace a properly-sized owned buffer with
            // an undersized upstream one (happens on the first frame of a
            // feedback loop when the downstream node hasn't produced data yet).
            if(binding.has_vertex_count_spec && ssbo.owned && ssbo.buffer)
            {
              const int elem_size = glslTypeSizeBytes(req.type);
              const int attr_count = ssbo.per_instance ? binding.instance_count : binding.vertex_count;
              const int64_t needed = (int64_t)elem_size * attr_count;
              if(needed > 0 && gpu->byte_size < needed)
              {
                continue;
              }
            }

            // For feedback receivers, never adopt upstream buffers for read_write
            // attributes. The node uses its own ping-pong pair; upstream data is
            // this node's own previous output routed through feedback.
            if(binding.is_feedback_receiver && req.access == "read_write")
            {
              continue;
            }

            if(ssbo.buffer != rhi_buf)
            {
              if(ssbo.owned && ssbo.buffer)
              {
                renderer.releaseBuffer(ssbo.buffer);
              }
              ssbo.buffer = rhi_buf;
              ssbo.size = gpu->byte_size;
              ssbo.owned = false;
              ssbo.lastUploadSrc = nullptr;
            }
            continue;
          }
          // AoS GPU buffer or format mismatch (e.g. float3→vec4): would need
          // scatter compute pass — not yet supported. Fall through to create
          // a fallback buffer instead of silently binding misaligned data.
          qWarning() << "CSF geometry: GPU buffer scatter not yet implemented for"
                      << req.name.c_str()
                      << "(upstream_size=" << attr_size << "shader_size=" << elem_size
                      << "stride=" << stride << ")";
          // Don't continue — fall through to create a fallback buffer below
        }

        if(auto* cpu = ossia::get_if<ossia::geometry::cpu_buffer>(&geo_buf.data))
        {
          if(!cpu->raw_data || cpu->byte_size <= 0)
            continue;

          const auto* src = static_cast<const char*>(cpu->raw_data.get());
          const int64_t elem_size = glslTypeSizeBytes(req.type);
          const int data_count = ssbo.per_instance ? mesh.instances : mesh.vertices;
          const int64_t needed = elem_size * data_count;

          // Skip re-upload if we already own a correctly-sized buffer
          // and the upstream data hasn't changed (same CPU pointer as last upload).
          // This avoids expensive per-frame scatter+upload for static geometry.
          if(ssbo.buffer && ssbo.owned && ssbo.size >= needed && ssbo.lastUploadSrc == src)
            continue;

          // Create or resize the SSBO
          if(!ssbo.buffer || ssbo.size < needed || !ssbo.owned)
          {
            if(ssbo.owned && ssbo.buffer)
            {
              renderer.releaseBuffer(ssbo.buffer);
            }
            auto* buf = renderer.state.rhi->newBuffer(
                QRhiBuffer::Static,
                QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer, needed);
            qWarning() << "CSF ALLOC [geomUpload]" << req.name.c_str() << "size=" << needed;
            buf->setName(QByteArray("CSF_Geom_") + req.name.c_str());
            buf->create();
            ssbo.buffer = buf;
            ssbo.size = needed;
            ssbo.owned = true;
          }

          // Total byte offset into the buffer: input entry offset + attribute offset within stride
          const int64_t base_offset = input_byte_offset + geo_attr->byte_offset;

          if(is_soa && attr_size == (int)elem_size)
          {
            // SoA CPU buffer with matching element size: upload directly
            const int64_t upload_size = std::min(needed, cpu->byte_size - base_offset);
            if(upload_size > 0)
              res.uploadStaticBuffer(ssbo.buffer, 0, upload_size, src + base_offset);
          }
          else if(m_gpuScatterAvailable)
          {
            // GPU scatter: upload raw data to staging SSBO, dispatch compute to convert.
            // This avoids expensive per-element CPU memcpy for large point clouds.
            const int64_t raw_size = (int64_t)data_count * stride;
            const int64_t staging_needed = base_offset + raw_size;

            if(!ssbo.scatterStaging || ssbo.scatterStagingSize < staging_needed)
            {
              delete ssbo.scatterStaging;
              ssbo.scatterStaging = renderer.state.rhi->newBuffer(
                  QRhiBuffer::Static, QRhiBuffer::StorageBuffer, staging_needed);
              ssbo.scatterStaging->setName(QByteArray("CSF_ScatterStaging_") + req.name.c_str());
              ssbo.scatterStaging->create();
              ssbo.scatterStagingSize = staging_needed;
            }

            // Bulk upload the raw CPU data as-is (no per-element processing)
            const int64_t upload_size = std::min(staging_needed, cpu->byte_size);
            res.uploadStaticBuffer(ssbo.scatterStaging, 0, upload_size, src);

            // Prepare the scatter dispatch (will execute in runInitialPasses)
            ssbo.scatterParams = GPUBufferScatter::Params{
                .staging = ssbo.scatterStaging,
                .output = ssbo.buffer,
                .element_count = (uint32_t)data_count,
                .src_components = (uint32_t)(attr_size / sizeof(float)),
                .dst_components = (uint32_t)(elem_size / sizeof(float)),
                .src_stride_floats = (uint32_t)(stride / sizeof(float)),
                .src_offset_floats = (uint32_t)(base_offset / sizeof(float)),
            };

            if(!ssbo.scatterOp.srb)
              ssbo.scatterOp = m_gpuScatter.prepare(*renderer.state.rhi, ssbo.scatterParams);

            ssbo.scatterPending = true;
          }
          else
          {
            // CPU fallback: scatter per-element with format conversion
            // (used when compute shaders are not available)
            QByteArray scattered(needed, 0);

            if(elem_size > attr_size && elem_size >= (int)sizeof(float))
            {
              const float one = 1.0f;
              for(int i = 0; i < data_count; i++)
                std::memcpy(scattered.data() + (int64_t)i * elem_size + elem_size - sizeof(float),
                            &one, sizeof(float));
            }

            const int copy_size = std::min((int)elem_size, attr_size);
            for(int i = 0; i < data_count; i++)
            {
              const int64_t src_off = (int64_t)i * stride + base_offset;
              if(src_off + copy_size <= cpu->byte_size)
                std::memcpy(scattered.data() + (int64_t)i * elem_size, src + src_off, copy_size);
            }
            res.uploadStaticBuffer(ssbo.buffer, 0, needed, scattered.constData());
          }
          ssbo.lastUploadSrc = src;
        }
      }

      // Handle auxiliary SSBOs: match against geometry's auxiliary buffers by name.
      // For feedback receivers, keep our own auxiliary SSBOs — the upstream data
      // traveled through the feedback loop and intermediate nodes may have created
      // their own buffers for auxiliaries they didn't receive (e.g. ColorByLife
      // doesn't forward dead/alive_b, so ParticleFeedback creates empty ones).
      // Adopting those would replace our live data with zeros.
      for(auto& aux : binding.auxiliary_ssbos)
      {
        if(binding.is_feedback_receiver)
          continue;

        if(auto* geo_aux = mesh.find_auxiliary(aux.name))
        {
          if(geo_aux->buffer >= 0 && geo_aux->buffer < (int)mesh.buffers.size())
          {
            const auto& geo_buf = mesh.buffers[geo_aux->buffer];
            if(auto* gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&geo_buf.data))
            {
              if(gpu->handle)
              {
                auto* rhi_buf = static_cast<QRhiBuffer*>(gpu->handle);
                if(aux.buffer != rhi_buf)
                {
                  if(aux.owned && aux.buffer)
                  {
                    renderer.releaseBuffer(aux.buffer);
                  }
                  aux.buffer = rhi_buf;
                  aux.size = geo_aux->byte_size > 0 ? geo_aux->byte_size : gpu->byte_size;
                  aux.owned = false;
                }
                continue;
              }
            }
          }
        }

        // No match from upstream geometry — create/resize our own buffer if no size_expr
        if(aux.size_expr.empty())
        {
          if(!aux.owned && aux.buffer)
          {
            aux.buffer = nullptr;
            aux.owned = true;
          }

          const int64_t requiredSize = score::gfx::calculateStorageBufferSize(
              aux.layout, 0, this->n.descriptor());
          if(!aux.buffer || aux.size < requiredSize)
          {
            if(aux.owned && aux.buffer)
            {
              renderer.releaseBuffer(aux.buffer);
            }
            auto* buf = renderer.state.rhi->newBuffer(
                QRhiBuffer::Static,
                QRhiBuffer::StorageBuffer, requiredSize);
            qWarning() << "CSF ALLOC [geoAuxNoMatch]" << aux.name.c_str() << "size=" << requiredSize;
            buf->setName(QByteArray("CSF_GeoAux_") + aux.name.c_str());
            buf->create();
            QByteArray zero(requiredSize, 0);
            res.uploadStaticBuffer(buf, 0, requiredSize, zero.constData());
            aux.buffer = buf;
            aux.size = requiredSize;
            aux.owned = true;
          }
        }
      }

      // When has_vertex_count_spec AND the upstream is a feedback loop (our own
      // SSBOs came back as gpu handles, identity check kept them owned), we must
      // still resize if $USER changed. Without this, the SSBOs stay at whatever
      // size was allocated during init() (possibly 1 element).
      if((binding.has_vertex_count_spec && binding.vertex_count > 0)
         || (binding.has_instance_count_spec && binding.instance_count > 0))
      {
        for(int attr_idx = 0; attr_idx < (int)geo_input->attributes.size(); attr_idx++)
        {
          if(attr_idx >= (int)binding.attribute_ssbos.size())
            break;
          auto& ssbo = binding.attribute_ssbos[attr_idx];
          if(!ssbo.owned || !ssbo.buffer)
          {
            continue;
          }
          const int elem_size = glslTypeSizeBytes(geo_input->attributes[attr_idx].type);
          const int attr_count = ssbo.per_instance ? binding.instance_count : binding.vertex_count;
          const int64_t needed = (int64_t)elem_size * attr_count;
          if(needed > 0 && ssbo.size != needed)
          {
            ssbo.buffer->destroy();
            ssbo.buffer->setSize(needed);
            ssbo.buffer->create();
            QByteArray zero(needed, 0);
            res.uploadStaticBuffer(ssbo.buffer, 0, needed, zero.constData());
            ssbo.size = needed;

            // Keep read_buffer in sync for feedback receivers
            if(binding.is_feedback_receiver && ssbo.read_buffer)
            {
              ssbo.read_buffer->destroy();
              ssbo.read_buffer->setSize(needed);
              ssbo.read_buffer->create();
              res.uploadStaticBuffer(ssbo.read_buffer, 0, needed, zero.constData());
            }
          }
        }
      }
    }
    else if(binding.has_vertex_count_spec)
    {
      // No upstream geometry, but vertex_count expression provides the count.
      // Clear stale unowned pointers first — upstream may have freed them.
      for(auto& ssbo : binding.attribute_ssbos)
      {
        if(!ssbo.owned)
        {
          ssbo.buffer = nullptr;
          ssbo.owned = true;
        }
      }
      for(auto& aux : binding.auxiliary_ssbos)
      {
        if(!aux.owned)
        {
          aux.buffer = nullptr;
          aux.owned = true;
        }
      }

      // Create/resize attribute SSBOs based on resolved count.
      if(binding.vertex_count > 0 || binding.instance_count > 0)
      {
        bool resized = false;
        for(int attr_idx = 0; attr_idx < (int)geo_input->attributes.size(); attr_idx++)
        {
          if(attr_idx >= (int)binding.attribute_ssbos.size())
            break;
          const auto& req = geo_input->attributes[attr_idx];
          auto& ssbo = binding.attribute_ssbos[attr_idx];
          if(req.access == "none")
            continue;
          const int count = ssbo.per_instance ? binding.instance_count : binding.vertex_count;
          if(count <= 0)
            continue;
          const int elem_size = glslTypeSizeBytes(req.type);
          const int64_t needed = (int64_t)elem_size * count;

          if(!ssbo.buffer || ssbo.size != needed)
          {
            if(ssbo.owned && ssbo.buffer)
            {
              ssbo.buffer->destroy();
              ssbo.buffer->setSize(needed);
              ssbo.buffer->create();
            }
            else
            {
              auto* buf = renderer.state.rhi->newBuffer(
                  QRhiBuffer::Static,
                  QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer, needed);
              qWarning() << "CSF ALLOC [geomSpecResize]" << req.name.c_str() << "size=" << needed;
              buf->setName(QByteArray("CSF_GeomSpec_") + req.name.c_str());
              buf->create();
              ssbo.buffer = buf;
              ssbo.owned = true;
            }
            QByteArray zero(needed, 0);
            res.uploadStaticBuffer(ssbo.buffer, 0, needed, zero.constData());
            ssbo.size = needed;
            resized = true;

            // Keep read_buffer in sync for feedback receivers
            if(binding.is_feedback_receiver && ssbo.read_buffer)
            {
              ssbo.read_buffer->destroy();
              ssbo.read_buffer->setSize(needed);
              ssbo.read_buffer->create();
              res.uploadStaticBuffer(ssbo.read_buffer, 0, needed, zero.constData());
            }
          }
        }

        // When attribute buffers are resized, zero-fill auxiliary buffers
        // to force shader re-initialization (e.g. particle dead lists).
        if(resized)
        {
          for(auto& aux : binding.auxiliary_ssbos)
          {
            if(aux.buffer && aux.size > 0)
            {
              QByteArray zero(aux.size, 0);
              res.uploadStaticBuffer(aux.buffer, 0, aux.size, zero.constData());
            }
          }
        }
      }
    }

    geo_binding_idx++;
  }
}

void RenderedCSFNode::pushOutputGeometry(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge& edge)
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

    // Determine upstream geometry for this binding
    const ossia::geometry* binding_upstream = nullptr;
    if(binding.input_port_index >= 0)
    {
      auto it = m_portGeometries.find(binding.input_port_index);
      if(it != m_portGeometries.end()
         && it->second.meshes && !it->second.meshes->meshes.empty())
      {
        binding_upstream = &it->second.meshes->meshes[0];
      }
    }

    // Count upstream pass-through attributes for structural change detection
    int upstream_attr_count = 0;
    if(binding_upstream)
    {
      for(const auto& in_attr : binding_upstream->attributes)
      {
        bool already_declared = false;
        for(const auto& req : geo_input->attributes)
        {
          const auto sem = ossia::name_to_semantic(req.semantic);
          if(in_attr.semantic != ossia::attribute_semantic::custom)
          {
            if(sem == in_attr.semantic) { already_declared = true; break; }
          }
          else
          {
            if(req.name == in_attr.name) { already_declared = true; break; }
          }
        }
        if(!already_declared)
          upstream_attr_count++;
      }
    }

    // Detect structural changes that require rebuilding the geometry from scratch
    const int cur_attr_count = (int)geo_input->attributes.size();
    bool structure_changed =
        !binding.outputGeometry.meshes
        || binding.prev_vertex_count != binding.vertex_count
        || binding.prev_instance_count != binding.instance_count
        || binding.prev_attribute_count != cur_attr_count
        || binding.prev_upstream_attr_count != upstream_attr_count;

    if(structure_changed)
    {
      qDebug() << "CSF pushOutput: STRUCTURAL REBUILD for" << input.name.c_str();
      // Full rebuild: allocate new mesh_list and populate from scratch
      auto meshes = std::make_shared<ossia::mesh_list>();
      ossia::geometry out_geo;
      out_geo.vertices = binding.vertex_count;
      out_geo.instances = binding.instance_count;
      out_geo.topology = ossia::geometry::points;
      out_geo.cull_mode = ossia::geometry::none;
      out_geo.front_face = ossia::geometry::counter_clockwise;

      if(binding_upstream)
      {
        out_geo.bounds = binding_upstream->bounds;
        // Inherit topology from upstream for filter-type nodes
        out_geo.topology = (decltype(out_geo.topology))binding_upstream->topology;
        out_geo.cull_mode = (decltype(out_geo.cull_mode))binding_upstream->cull_mode;
        out_geo.front_face = (decltype(out_geo.front_face))binding_upstream->front_face;
      }

      for(int attr_idx = 0; attr_idx < (int)geo_input->attributes.size(); attr_idx++)
      {
        if(attr_idx >= (int)binding.attribute_ssbos.size())
          break;

        const auto& req = geo_input->attributes[attr_idx];
        auto& ssbo = binding.attribute_ssbos[attr_idx];

        if(!ssbo.buffer)
          continue;

        const int buf_index = (int)out_geo.buffers.size();
        const int elem_size = glslTypeSizeBytes(req.type);

        ossia::geometry::buffer buf{
            .data = ossia::geometry::gpu_buffer{ssbo.buffer, ssbo.size},
            .dirty = false};
        out_geo.buffers.push_back(std::move(buf));

        ossia::geometry::binding bind;
        bind.byte_stride = elem_size;
        bind.classification = ssbo.per_instance
            ? ossia::geometry::binding::per_instance
            : ossia::geometry::binding::per_vertex;
        bind.step_rate = ssbo.per_instance ? 1 : 0;
        out_geo.bindings.push_back(bind);

        ossia::geometry::attribute attr;
        attr.binding = buf_index;
        attr.location = attr_idx;
        const auto sem = ossia::name_to_semantic(req.semantic);
        attr.semantic = sem;
        if(sem == ossia::attribute_semantic::custom)
          attr.name = req.name;

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

      // Forward upstream attributes not declared by this node
      if(binding_upstream)
      {
        const auto& in_mesh = *binding_upstream;
        for(const auto& in_attr : in_mesh.attributes)
        {
          bool already_present = false;
          for(const auto& out_attr : out_geo.attributes)
          {
            if(in_attr.semantic != ossia::attribute_semantic::custom)
            {
              if(out_attr.semantic == in_attr.semantic) { already_present = true; break; }
            }
            else
            {
              if(out_attr.name == in_attr.name) { already_present = true; break; }
            }
          }
          if(already_present)
            continue;

          // Look up the actual buffer through the input array (binding→buffer indirection)
          const int in_binding_idx = in_attr.binding;
          if(in_binding_idx < 0 || in_binding_idx >= (int)in_mesh.input.size())
            continue;
          const auto& in_inp = in_mesh.input[in_binding_idx];
          if(in_inp.buffer < 0 || in_inp.buffer >= (int)in_mesh.buffers.size())
            continue;

          const int buf_index = (int)out_geo.buffers.size();
          out_geo.buffers.push_back(in_mesh.buffers[in_inp.buffer]);

          ossia::geometry::binding bind;
          if(in_binding_idx < (int)in_mesh.bindings.size())
            bind = in_mesh.bindings[in_binding_idx];
          else
            bind.classification = ossia::geometry::binding::per_vertex;
          out_geo.bindings.push_back(bind);

          ossia::geometry::attribute attr = in_attr;
          attr.binding = buf_index;
          attr.location = (int)out_geo.attributes.size();
          out_geo.attributes.push_back(attr);

          struct ossia::geometry::input inp;
          inp.buffer = buf_index;
          inp.byte_offset = in_inp.byte_offset;
          out_geo.input.push_back(inp);
        }
      }

      // Attach geometry-level auxiliary SSBOs
      for(const auto& aux : binding.auxiliary_ssbos)
      {
        if(aux.buffer)
        {
          const int aux_buf_idx = (int)out_geo.buffers.size();
          out_geo.buffers.push_back({
              .data = ossia::geometry::gpu_buffer{aux.buffer, aux.size},
              .dirty = false});

          out_geo.auxiliary.push_back({
              .name = aux.name, .buffer = aux_buf_idx,
              .byte_offset = 0, .byte_size = aux.size});
        }
      }

      // Attach standalone storage buffers as auxiliary
      for(const auto& sb : m_storageBuffers)
      {
        if(sb.buffer)
        {
          const int aux_buf_idx = (int)out_geo.buffers.size();
          out_geo.buffers.push_back({
              .data = ossia::geometry::gpu_buffer{sb.buffer, sb.size},
              .dirty = false});

          out_geo.auxiliary.push_back({
              .name = sb.name.toStdString(), .buffer = aux_buf_idx,
              .byte_offset = 0, .byte_size = sb.size});
        }
      }

      // Forward upstream auxiliary buffers from this binding's own upstream only.
      // (Single-geometry case: implicit forwarding for pass-through nodes.)
      if(binding_upstream)
      {
        for(const auto& in_aux : binding_upstream->auxiliary)
        {
          bool already_present = false;
          for(const auto& existing : out_geo.auxiliary)
            if(existing.name == in_aux.name) { already_present = true; break; }
          if(already_present)
            continue;

          if(in_aux.buffer >= 0 && in_aux.buffer < (int)binding_upstream->buffers.size())
          {
            const int aux_buf_idx = (int)out_geo.buffers.size();
            out_geo.buffers.push_back(binding_upstream->buffers[in_aux.buffer]);
            out_geo.auxiliary.push_back({
                .name = in_aux.name, .buffer = aux_buf_idx,
                .byte_offset = in_aux.byte_offset, .byte_size = in_aux.byte_size});
          }
        }
      }

      // Explicit COPY_FROM: forward auxiliary buffers from other geometries
      for(const auto& aux_req : geo_input->auxiliary)
      {
        if(!aux_req.forward)
          continue;

        // Already attached by this binding's own auxiliary SSBOs?
        bool already_present = false;
        for(const auto& existing : out_geo.auxiliary)
          if(existing.name == aux_req.name) { already_present = true; break; }
        if(already_present)
          continue;

        // Find the source geometry by name
        const std::string& src_geo_name = aux_req.forward->geometry;
        const std::string src_aux_name
            = aux_req.forward->auxiliary.empty() ? aux_req.name : aux_req.forward->auxiliary;

        // Search all input port geometries for the source
        for(const auto& [port_idx, geo_spec] : m_portGeometries)
        {
          if(!geo_spec.meshes || geo_spec.meshes->meshes.empty())
            continue;

          // Match by geometry resource name → find the binding with that name
          int src_binding_idx = 0;
          bool found_geo = false;
          for(const auto& inp : n.m_descriptor.inputs)
          {
            auto* src_geo = ossia::get_if<isf::geometry_input>(&inp.data);
            if(!src_geo) continue;
            if(src_binding_idx >= (int)m_geometryBindings.size()) break;
            auto& src_binding = m_geometryBindings[src_binding_idx];
            if(inp.name == src_geo_name && src_binding.input_port_index == port_idx)
            {
              found_geo = true;
              break;
            }
            src_binding_idx++;
          }

          if(!found_geo)
            continue;

          const auto& src_mesh = geo_spec.meshes->meshes[0];
          if(auto* src_aux = src_mesh.find_auxiliary(src_aux_name))
          {
            if(src_aux->buffer >= 0 && src_aux->buffer < (int)src_mesh.buffers.size())
            {
              const int aux_buf_idx = (int)out_geo.buffers.size();
              out_geo.buffers.push_back(src_mesh.buffers[src_aux->buffer]);
              out_geo.auxiliary.push_back({
                  .name = aux_req.name, .buffer = aux_buf_idx,
                  .byte_offset = src_aux->byte_offset, .byte_size = src_aux->byte_size});
              break;
            }
          }
        }
      }

      // Explicit COPY_FROM: forward attributes from other geometries
      for(const auto& attr_req : geo_input->attributes)
      {
        if(!attr_req.forward)
          continue;
        qDebug() << "CSF COPY_FROM: processing" << attr_req.name.c_str()
                 << "forward.geo=" << attr_req.forward->geometry.c_str()
                 << "forward.attr=" << attr_req.forward->attribute.c_str();

        // Already present in output?
        bool already_present = false;
        for(const auto& out_attr : out_geo.attributes)
        {
          const auto sem = ossia::name_to_semantic(attr_req.semantic);
          if(sem != ossia::attribute_semantic::custom && out_attr.semantic == sem)
          { already_present = true; break; }
          if(sem == ossia::attribute_semantic::custom && out_attr.name == attr_req.name)
          { already_present = true; break; }
        }
        if(already_present)
        {
          qDebug() << "CSF COPY_FROM:" << attr_req.name.c_str() << "already present, skipping";
          continue;
        }

        const std::string& src_geo_name = attr_req.forward->geometry;
        const std::string& src_attr_name = attr_req.forward->attribute;

        for(const auto& [port_idx, geo_spec] : m_portGeometries)
        {
          if(!geo_spec.meshes || geo_spec.meshes->meshes.empty())
            continue;

          // Find the matching source geometry binding
          int src_binding_idx = 0;
          bool found_geo = false;
          for(const auto& inp : n.m_descriptor.inputs)
          {
            auto* src_geo = ossia::get_if<isf::geometry_input>(&inp.data);
            if(!src_geo) continue;
            if(src_binding_idx >= (int)m_geometryBindings.size()) break;
            auto& src_binding = m_geometryBindings[src_binding_idx];
            if(inp.name == src_geo_name && src_binding.input_port_index == port_idx)
            {
              found_geo = true;
              break;
            }
            src_binding_idx++;
          }

          if(!found_geo)
          {
            qDebug() << "CSF COPY_FROM: geometry" << src_geo_name.c_str()
                     << "not matched at port" << port_idx;
            continue;
          }

          const auto& src_mesh = geo_spec.meshes->meshes[0];
          // Find the source attribute by name
          qDebug() << "CSF COPY_FROM: searching" << src_mesh.attributes.size()
                   << "attrs for" << src_attr_name.c_str();
          for(const auto& in_attr : src_mesh.attributes)
          {
            std::string_view attr_display = ossia::geometry::display_name(in_attr);
            qDebug() << "  checking sem=" << (int)in_attr.semantic
                     << "name=" << in_attr.name.c_str()
                     << "display=" << std::string(attr_display).c_str();
            bool name_match = (!src_attr_name.empty() && attr_display == src_attr_name);
            if(!name_match)
            {
              auto src_sem = ossia::name_to_semantic(src_attr_name);
              name_match = (src_sem != ossia::attribute_semantic::custom
                            && in_attr.semantic == src_sem);
            }
            if(!name_match)
              continue;

            const int in_binding_idx = in_attr.binding;
            if(in_binding_idx < 0 || in_binding_idx >= (int)src_mesh.input.size())
              break;
            const auto& in_inp = src_mesh.input[in_binding_idx];
            if(in_inp.buffer < 0 || in_inp.buffer >= (int)src_mesh.buffers.size())
              break;

            const int buf_index = (int)out_geo.buffers.size();

            // Upload CPU buffers to GPU so the output geometry is uniform.
            const auto& src_buf = src_mesh.buffers[in_inp.buffer];
            if(auto* cpu = ossia::get_if<ossia::geometry::cpu_buffer>(&src_buf.data))
            {
              if(cpu->raw_data && cpu->byte_size > 0)
              {
                auto& rhi = *renderer.state.rhi;
                auto* gpu_buf = rhi.newBuffer(
                    QRhiBuffer::Static,
                    QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer,
                    cpu->byte_size);
                gpu_buf->setName(QByteArray("CSF_CopyFrom_") + attr_req.name.c_str());
                gpu_buf->create();
                res.uploadStaticBuffer(gpu_buf, 0, cpu->byte_size, cpu->raw_data.get());
                qDebug() << "CSF COPY_FROM: UPLOAD" << attr_req.name.c_str()
                         << "size=" << cpu->byte_size;

                out_geo.buffers.push_back({
                    .data = ossia::geometry::gpu_buffer{gpu_buf, cpu->byte_size},
                    .dirty = false});
              }
              else
              {
                out_geo.buffers.push_back(src_buf);
              }
            }
            else
            {
              out_geo.buffers.push_back(src_buf);
            }

            // binding index = next slot in bindings array (NOT the buffer index)
            const int binding_index = (int)out_geo.bindings.size();

            ossia::geometry::binding bind;
            if(in_binding_idx < (int)src_mesh.bindings.size())
              bind = src_mesh.bindings[in_binding_idx];
            else
              bind.classification = ossia::geometry::binding::per_vertex;

            // Override classification from the COPY_FROM attribute's rate
            if(attr_req.rate == "instance")
            {
              bind.classification = ossia::geometry::binding::per_instance;
              bind.step_rate = 1;
            }
            out_geo.bindings.push_back(bind);

            ossia::geometry::attribute attr = in_attr;
            attr.binding = binding_index;
            attr.location = (int)out_geo.attributes.size();
            // Override semantic from the request
            auto sem = ossia::name_to_semantic(attr_req.semantic);
            attr.semantic = sem;
            // Always set the name so it can be matched by the shader variable name
            attr.name = attr_req.name;

            // Override format from the request type
            if(attr_req.type == "float") attr.format = ossia::geometry::attribute::float1;
            else if(attr_req.type == "vec2") attr.format = ossia::geometry::attribute::float2;
            else if(attr_req.type == "vec3") attr.format = ossia::geometry::attribute::float3;
            else if(attr_req.type == "vec4") attr.format = ossia::geometry::attribute::float4;

            out_geo.attributes.push_back(attr);

            struct ossia::geometry::input inp_out;
            inp_out.buffer = buf_index;
            inp_out.byte_offset = in_inp.byte_offset;
            out_geo.input.push_back(inp_out);

            break;
          }
          break;
        }
      }

      // Forward index buffer from upstream
      if(binding_upstream && binding_upstream->index.buffer >= 0
         && binding_upstream->index.buffer < (int)binding_upstream->buffers.size())
      {
        const int idx_buf_idx = (int)out_geo.buffers.size();
        out_geo.buffers.push_back(binding_upstream->buffers[binding_upstream->index.buffer]);
        out_geo.index.buffer = idx_buf_idx;
        out_geo.index.byte_offset = binding_upstream->index.byte_offset;
        out_geo.index.format = (decltype(out_geo.index.format))binding_upstream->index.format;
        out_geo.indices = binding_upstream->indices;
      }

#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
      if(binding.uses_indirect_draw && binding.indirectDrawBuffer)
      {
        out_geo.indirect_count = ossia::geometry::gpu_buffer{
            binding.indirectDrawBuffer,
            binding.indirect_draw_indexed
                ? (int64_t)sizeof(QRhiIndexedIndirectDrawCommand)
                : (int64_t)sizeof(QRhiIndirectDrawCommand)};
      }
#endif

      meshes->meshes.push_back(std::move(out_geo));
      meshes->dirty_index = 1; // Initial structural build

      binding.outputGeometry.meshes = meshes;
      binding.outputGeometry.filters = {};
      binding.prev_vertex_count = binding.vertex_count;
      binding.prev_instance_count = binding.instance_count;
      binding.prev_attribute_count = cur_attr_count;
      binding.prev_upstream_attr_count = upstream_attr_count;
    }
    else
    {
      // Fast path: same structure, just update buffer handles in place.
      // The mesh_list shared_ptr stays the same → downstream sees same pointer
      // → geometryChanged not set → no pipeline recreation.
      auto& out_geo = binding.outputGeometry.meshes->meshes[0];
      out_geo.vertices = binding.vertex_count;
      out_geo.instances = binding.instance_count;
      bool any_handle_changed = false;

      if(binding_upstream)
      {
        out_geo.bounds = binding_upstream->bounds;
        out_geo.topology = (decltype(out_geo.topology))binding_upstream->topology;
        out_geo.cull_mode = (decltype(out_geo.cull_mode))binding_upstream->cull_mode;
        out_geo.front_face = (decltype(out_geo.front_face))binding_upstream->front_face;

        // Update index buffer handle from upstream
        if(out_geo.index.buffer >= 0 && out_geo.index.buffer < (int)out_geo.buffers.size()
           && binding_upstream->index.buffer >= 0
           && binding_upstream->index.buffer < (int)binding_upstream->buffers.size())
        {
          const auto& src = binding_upstream->buffers[binding_upstream->index.buffer];
          auto& dst = out_geo.buffers[out_geo.index.buffer];
          if(auto* src_gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&src.data))
          {
            auto* dst_gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&dst.data);
            if(dst_gpu && (dst_gpu->handle != src_gpu->handle || dst_gpu->byte_size != src_gpu->byte_size))
            {
              dst_gpu->handle = src_gpu->handle;
              dst_gpu->byte_size = src_gpu->byte_size;
              dst.dirty = true;
              any_handle_changed = true;
            }
          }
          else
          {
            dst = src;
            dst.dirty = true;
            any_handle_changed = true;
          }
          out_geo.indices = binding_upstream->indices;
        }
      }

      // Update declared attribute buffer handles
      int buf_idx = 0;
      for(int attr_idx = 0; attr_idx < (int)geo_input->attributes.size(); attr_idx++)
      {
        if(attr_idx >= (int)binding.attribute_ssbos.size())
          break;
        auto& ssbo = binding.attribute_ssbos[attr_idx];
        if(!ssbo.buffer)
          continue;

        if(buf_idx < (int)out_geo.buffers.size())
        {
          auto& buf = out_geo.buffers[buf_idx];
          auto* gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&buf.data);
          if(gpu && (gpu->handle != ssbo.buffer || gpu->byte_size != ssbo.size))
          {
            gpu->handle = ssbo.buffer;
            gpu->byte_size = ssbo.size;
            buf.dirty = true;
            any_handle_changed = true;
          }
        }
        buf_idx++;
      }

      // Update pass-through attribute buffer handles
      if(binding_upstream)
      {
        const auto& in_mesh = *binding_upstream;
        for(const auto& in_attr : in_mesh.attributes)
        {
          bool already_declared = false;
          for(const auto& req : geo_input->attributes)
          {
            const auto sem = ossia::name_to_semantic(req.semantic);
            if(in_attr.semantic != ossia::attribute_semantic::custom)
            {
              if(sem == in_attr.semantic) { already_declared = true; break; }
            }
            else
            {
              if(req.name == in_attr.name) { already_declared = true; break; }
            }
          }
          if(already_declared)
            continue;

          const int in_binding_idx = in_attr.binding;
          if(in_binding_idx >= 0 && in_binding_idx < (int)in_mesh.input.size()
             && in_mesh.input[in_binding_idx].buffer >= 0
             && in_mesh.input[in_binding_idx].buffer < (int)in_mesh.buffers.size()
             && buf_idx < (int)out_geo.buffers.size())
          {
            auto& src = in_mesh.buffers[in_mesh.input[in_binding_idx].buffer];
            auto& dst = out_geo.buffers[buf_idx];
            if(auto* src_gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&src.data))
            {
              auto* dst_gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&dst.data);
              if(dst_gpu && (dst_gpu->handle != src_gpu->handle || dst_gpu->byte_size != src_gpu->byte_size))
              {
                dst_gpu->handle = src_gpu->handle;
                dst_gpu->byte_size = src_gpu->byte_size;
                dst.dirty = true;
                any_handle_changed = true;
              }
            }
            else
            {
              // CPU buffer — just copy
              dst = src;
              dst.dirty = true;
              any_handle_changed = true;
            }
          }
          buf_idx++;
        }
      }

      // Update auxiliary buffer handles
      int aux_buf_start = buf_idx;
      for(const auto& aux : binding.auxiliary_ssbos)
      {
        if(aux.buffer && buf_idx < (int)out_geo.buffers.size())
        {
          auto& buf = out_geo.buffers[buf_idx];
          auto* gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&buf.data);
          if(gpu && (gpu->handle != aux.buffer || gpu->byte_size != aux.size))
          {
            gpu->handle = aux.buffer;
            gpu->byte_size = aux.size;
            buf.dirty = true;
            any_handle_changed = true;
          }
          buf_idx++;
        }
      }

      for(const auto& sb : m_storageBuffers)
      {
        if(sb.buffer && buf_idx < (int)out_geo.buffers.size())
        {
          auto& buf = out_geo.buffers[buf_idx];
          auto* gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&buf.data);
          if(gpu && (gpu->handle != sb.buffer || gpu->byte_size != sb.size))
          {
            gpu->handle = sb.buffer;
            gpu->byte_size = sb.size;
            buf.dirty = true;
            any_handle_changed = true;
          }
          buf_idx++;
        }
      }

#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
      if(binding.uses_indirect_draw && binding.indirectDrawBuffer)
      {
        out_geo.indirect_count = ossia::geometry::gpu_buffer{
            binding.indirectDrawBuffer,
            binding.indirect_draw_indexed
                ? (int64_t)sizeof(QRhiIndexedIndirectDrawCommand)
                : (int64_t)sizeof(QRhiIndirectDrawCommand)};
      }
#endif

      // Only bump dirty_index if any handle actually changed,
      // so downstream acquireMesh picks up the new buffers.
      if(any_handle_changed)
      {
        qDebug() << "CSF pushOutput: FAST PATH handle changed for" << input.name.c_str();
        binding.outputGeometry.meshes->dirty_index++;
      }
    }

    // Push to downstream
    const auto& outlets = n.output;
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
        break;
      }
    }
    outlet_idx += geo_output_idx;

    if(!outlets.empty() && outlets[0]->type == Types::Image)
      outlet_idx++;

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
        rendered->second->process(port_idx, binding.outputGeometry);
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
      else if(it != m_storageBuffers.end())
      {
        if(!it->buffer) {
          qDebug() << "CSF: cannot bind null buffer";
        }
        bindingIndex++;
      }
      else
      {
        qDebug() << "CSF: storage buffer not found";
        bindingIndex++;
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

          if(!it->texture)
          {
            QRhiTexture* texture{};
            if(!image->depth_expression.empty())
            {
              // 3D texture
              int depth = resolveDispatchExpression(image->depth_expression);

              QRhiTexture::Flags flags
                  = QRhiTexture::ThreeDimensional | QRhiTexture::UsedWithLoadStore;
              texture = rhi.newTexture(format, imageSize.width(), imageSize.height(), depth, 1, flags);
              qWarning() << "CSF ALLOC [storageImage3D]" << input.name.c_str() << "size=" << imageSize.width() << "x" << imageSize.height() << "x" << depth;
            }
            else
            {
              // 2D texture
              QRhiTexture::Flags flags
                  = QRhiTexture::RenderTarget | QRhiTexture::UsedWithLoadStore
                    | QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips;
              texture = rhi.newTexture(format, imageSize, 1, flags);
              qWarning() << "CSF ALLOC [storageImage2D]" << input.name.c_str() << "size=" << imageSize;
            }
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
          else
          {
            bindingIndex++; // keep indices synchronized with shader layout
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

          // "none" access: forwarded via COPY_FROM, no binding needed
          if(req.access == "none")
            continue;

          if(!ssbo.buffer)
          {
            // Create a minimal fallback buffer so we don't crash
            const int elem_size = glslTypeSizeBytes(req.type);
            ssbo.buffer = rhi.newBuffer(
                QRhiBuffer::Static,
                QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer, elem_size);
            qWarning() << "CSF ALLOC [geomInit]" << req.name.c_str() << "size=" << elem_size;
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
          else // read_write -> 2 bindings: _in (readonly) + _out (writeonly)
          {
            // On the first feedback frame (pending_initial_copy), use the same
            // buffer for both _in and _out so the shader can init + simulate
            // in the same frame.  After the frame we copy buffer→read_buffer.
            QRhiBuffer* read_buf = (ssbo.read_buffer && !binding.pending_initial_copy)
                ? ssbo.read_buffer : ssbo.buffer;
            if(read_buf == ssbo.buffer)
            {
              // Same physical buffer for both _in and _out (non-feedback in-place).
              // Use bufferLoadStore for both to avoid "different accesses" validation error.
              bindings.append(QRhiShaderResourceBinding::bufferLoadStore(
                  bindingIndex++, QRhiShaderResourceBinding::ComputeStage, ssbo.buffer));
              bindings.append(QRhiShaderResourceBinding::bufferLoadStore(
                  bindingIndex++, QRhiShaderResourceBinding::ComputeStage, ssbo.buffer));
            }
            else
            {
              // Distinct buffers (feedback receiver): _in readonly, _out read-write
              bindings.append(QRhiShaderResourceBinding::bufferLoad(
                  bindingIndex++, QRhiShaderResourceBinding::ComputeStage, read_buf));
              bindings.append(QRhiShaderResourceBinding::bufferLoadStore(
                  bindingIndex++, QRhiShaderResourceBinding::ComputeStage, ssbo.buffer));
            }
          }
        }

        // Auxiliary SSBOs for this geometry input
        for(auto& aux : binding.auxiliary_ssbos)
        {
          if(!aux.buffer)
          {
            // Create a minimal fallback buffer so we don't skip a binding index
            aux.buffer = rhi.newBuffer(
                QRhiBuffer::Static, QRhiBuffer::StorageBuffer, 16);
            qWarning() << "CSF ALLOC [auxInit]" << aux.name.c_str() << "size=16";
            aux.buffer->setName(QByteArray("CSF_AuxInit_") + aux.name.c_str());
            aux.buffer->create();
            aux.size = 16;
            aux.owned = true;
          }

          if(aux.access == "read_only")
          {
            bindings.append(QRhiShaderResourceBinding::bufferLoad(
                bindingIndex++, QRhiShaderResourceBinding::ComputeStage, aux.buffer));
          }
          else if(aux.access == "write_only")
          {
            bindings.append(QRhiShaderResourceBinding::bufferStore(
                bindingIndex++, QRhiShaderResourceBinding::ComputeStage, aux.buffer));
          }
          else
          {
            bindings.append(QRhiShaderResourceBinding::bufferLoadStore(
                bindingIndex++, QRhiShaderResourceBinding::ComputeStage, aux.buffer));
          }
        }

#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
        // Bind indirect draw buffer as read-write SSBO
        if(binding.uses_indirect_draw && binding.indirectDrawBuffer)
        {
          bindings.append(QRhiShaderResourceBinding::bufferLoadStore(
              bindingIndex++, QRhiShaderResourceBinding::ComputeStage,
              binding.indirectDrawBuffer));
        }
#endif

        geo_binding_index++;
      }
      // Inlet port if any attribute reads from upstream
      for(const auto& attr : geo_input->attributes)
        if(attr.access == "read_only" || attr.access == "read_write") { input_port_index++; break; }
      // Skip $USER ports for this geometry input
      if(geo_input->vertex_count.find("$USER") != std::string::npos) input_port_index++;
      if(geo_input->instance_count.find("$USER") != std::string::npos) input_port_index++;
      for(const auto& aux : geo_input->auxiliary)
        if(aux.size.find("$USER") != std::string::npos) input_port_index++;
    }
    else
    {
      input_port_index++;
    }
  }

  // Set the SRB on the pipeline and create it
  {
    QRhiShaderResourceBindings* passSRB{};
    // Create one ComputePass entry for each CSF pass, each with their own pipeline, ProcessUBO and SRB
    for(std::size_t passIdx = 0; passIdx < n.m_descriptor.csf_passes.size(); passIdx++)
    {
      // Create a separate ProcessUBO for this pass
      QRhiBuffer* passProcessUBO = rhi.newBuffer(
          QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
      qWarning() << "CSF ALLOC [passProcessUBO] pass=" << passIdx << "size=" << sizeof(ProcessUBO);
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
      qWarning() << "CSF ALLOC [passSRB] pass=" << passIdx;
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

      auto* passPipeline = (passIdx < m_perPassPipelines.size())
                              ? m_perPassPipelines[passIdx]
                              : m_computePipeline;

      // Each pipeline needs its SRB set and finalized
      passPipeline->setShaderResourceBindings(passSRB);
      if(!passPipeline->create())
      {
        qWarning() << "Failed to create compute pipeline for pass" << passIdx;
        delete passSRB;
        delete passProcessUBO;
        continue;
      }

      m_computePasses.emplace_back(
          &edge, ComputePass{passPipeline, passSRB, passProcessUBO});
    }

    if(rt.renderTarget)
    {
      // Create the graphics pass for rendering this output to the render target
      createGraphicsPass(rt, renderer, edge, res);
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

  // Find the texture for the specific output port this edge is connected to
  QRhiTexture* textureToRender = textureForOutput(*edge.source);
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
    // Prepare the shader template with image format substitution.
    // LOCAL_SIZE placeholders will be substituted per-pass below.
    m_computeShaderSource = updateShaderWithImageFormats(n.m_computeS);

    // Compile one pipeline per pass (each may have a different LOCAL_SIZE)
    m_perPassPipelines.clear();
    for(std::size_t passIdx = 0; passIdx < n.m_descriptor.csf_passes.size(); passIdx++)
    {
      const auto& passDesc = n.m_descriptor.csf_passes[passIdx];

      // Substitute LOCAL_SIZE placeholders
      QString src = m_computeShaderSource;
      src.replace("ISF_LOCAL_SIZE_X", QString::number(passDesc.local_size[0]));
      src.replace("ISF_LOCAL_SIZE_Y", QString::number(passDesc.local_size[1]));
      src.replace("ISF_LOCAL_SIZE_Z", QString::number(passDesc.local_size[2]));

      QShader compiled = score::gfx::makeCompute(renderer.state, src);

      auto* pipeline = rhi.newComputePipeline();
      pipeline->setShaderStage(QRhiShaderStage(QRhiShaderStage::Compute, compiled));

      m_perPassPipelines.push_back(pipeline);
    }

    // For backward compat: point m_computePipeline to the first pass
    m_computePipeline = m_perPassPipelines.empty() ? nullptr : m_perPassPipelines[0];
    if(!m_perPassPipelines.empty())
      m_computeShader = m_perPassPipelines[0]->shaderStage().shader();
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

  // Initialize GPU buffer scatter for format conversion
  m_gpuScatterAvailable = m_gpuScatter.init(renderer.state);

  // Create the material UBO
  m_materialSize = n.m_materialSize;
  if(m_materialSize > 0)
  {
    m_materialUBO = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, m_materialSize);
    qWarning() << "CSF ALLOC [materialUBO] size=" << m_materialSize;
    m_materialUBO->setName("RenderedCSFNode::init::m_materialUBO");
    if(!m_materialUBO->create())
    {
      qWarning() << "Failed to create uniform buffer";
      delete m_materialUBO;
      m_materialUBO = nullptr;
    }
  }

  // Initialize input samplers
  SCORE_ASSERT(m_computePasses.empty());
  SCORE_ASSERT(m_inputSamplers.empty());

  // Create samplers for input textures
  m_inputSamplers = initInputSamplers(this->n, renderer, n.input);

  // Parse descriptor to create storage buffers and determine output texture requirements.
  // We also track the input port index to build the geometry-binding-to-port mapping.
  // The input port index mirrors the order in which ISFNode's visitor calls
  // self.input.push_back() for each descriptor input.
  int sb_index = 0;
  int outlet_index = 0;
  int input_port_index = 0; // tracks which input port we're at
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
      sb.buffer_usage = storage->buffer_usage;
      sb.access = QString::fromStdString(storage->access);
      sb.layout = storage->layout; // Store layout for size calculation
      m_storageBuffers.push_back(sb);

      if(sb.access.contains("write")) {
        m_outStorageBuffers.push_back({outlets[outlet_index], sb_index});
        outlet_index++;
      }
      // read_only storage creates an input port
      if(storage->access == "read_only")
        input_port_index++;
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
        int img_index = (int)m_storageImages.size() - 1;
        m_outStorageImages.push_back({outlets[outlet_index], img_index});
        outlet_index++;
      }
      // read_only CSF image creates an input port
      if(image->access == "read_only")
        input_port_index++;
    }
    // Handle geometry inputs
    else if(auto* geo = ossia::get_if<isf::geometry_input>(&input.data))
    {
      // Determine if this geometry_input creates an input port
      // (mirrors ISFNode visitor logic: input port if any attribute is read_only or read_write)
      bool needs_input = geo->attributes.empty(); // empty = pass-through, always has input
      if(!needs_input)
      {
        for(const auto& attr : geo->attributes)
          if(attr.access == "read_only" || attr.access == "read_write")
          { needs_input = true; break; }
      }

      GeometryBinding binding;
      binding.input_port_index = needs_input ? input_port_index : -1;
      binding.has_output = geo->attributes.empty(); // Empty attributes = pure pass-through with output
      binding.has_vertex_count_spec = !geo->vertex_count.empty();
      binding.has_instance_count_spec = !geo->instance_count.empty();

      for(const auto& attr : geo->attributes)
      {
        GeometryBinding::AttributeSSBO ssbo;
        ssbo.name = attr.name;
        ssbo.access = attr.access;
        ssbo.per_instance = (attr.rate == "instance");
        binding.attribute_ssbos.push_back(std::move(ssbo));

        if(attr.access != "read_only" && attr.access != "none")
          binding.has_output = true;
      }

      // If vertex_count is specified, resolve and pre-allocate attribute SSBOs
      if(binding.has_vertex_count_spec)
      {
        int count = resolveCountExpression(geo->vertex_count, *geo, "vertex_count");
        if(count > 0)
          binding.vertex_count = count;
      }

      // Resolve instance_count if specified
      if(binding.has_instance_count_spec)
      {
        int ic = resolveCountExpression(geo->instance_count, *geo, "instance_count");
        if(ic > 0)
          binding.instance_count = ic;
      }

      // Pre-allocate attribute SSBOs using the correct count based on rate
      {
        for(int attr_idx = 0; attr_idx < (int)geo->attributes.size(); attr_idx++)
        {
          if(attr_idx >= (int)binding.attribute_ssbos.size())
            break;
          auto& ssbo = binding.attribute_ssbos[attr_idx];
          if(ssbo.access == "none")
            continue;
          const int count = ssbo.per_instance ? binding.instance_count : binding.vertex_count;
          if(count <= 0)
            continue;
          const int elem_size = glslTypeSizeBytes(geo->attributes[attr_idx].type);
          const int64_t needed = (int64_t)elem_size * count;
          auto* buf = rhi.newBuffer(
              QRhiBuffer::Static,
              QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer, needed);
          qWarning() << "CSF ALLOC [geomSpecInit]" << ssbo.name.c_str() << "size=" << needed;
          buf->setName(QByteArray("CSF_GeomSpec_") + ssbo.name.c_str());
          buf->create();
          QByteArray zero(needed, 0);
          res.uploadStaticBuffer(buf, 0, needed, zero.constData());
          ssbo.buffer = buf;
          ssbo.size = needed;
          ssbo.owned = true;
        }
      }

      for(const auto& aux : geo->auxiliary)
      {
        // COPY_FROM auxiliaries are forwarded in pushOutputGeometry, no SSBO needed
        if(aux.forward)
          continue;

        GeometryBinding::AuxiliarySSBO ssbo;
        ssbo.name = aux.name;
        ssbo.access = aux.access;
        ssbo.layout = aux.layout;
        ssbo.size_expr = aux.size;

        // Create the buffer immediately so it's available for the first dispatch
        int arrayCount = 0;
        if(!aux.size.empty())
          arrayCount = resolveCountExpression(aux.size, *geo, aux.name);

        const int64_t requiredSize = score::gfx::calculateStorageBufferSize(
            aux.layout, arrayCount, this->n.descriptor());
        if(requiredSize > 0)
        {
          auto* buf = rhi.newBuffer(
              QRhiBuffer::Static,
              QRhiBuffer::StorageBuffer, requiredSize);
          qWarning() << "CSF ALLOC [geoAuxInit]" << aux.name.c_str() << "size=" << requiredSize;
          buf->setName(QByteArray("CSF_GeoAux_") + aux.name.c_str());
          buf->create();
          QByteArray zero(requiredSize, 0);
          res.uploadStaticBuffer(buf, 0, requiredSize, zero.constData());
          ssbo.buffer = buf;
          ssbo.size = requiredSize;
          ssbo.owned = true;
        }

        binding.auxiliary_ssbos.push_back(std::move(ssbo));

        if(aux.access != "read_only")
          binding.has_output = true;
      }

#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
      // Allocate indirect draw buffer if requested
      if(geo->indirect_draw && renderer.state.caps.drawIndirect)
      {
        binding.uses_indirect_draw = true;
        binding.indirect_draw_indexed = (geo->indirect_draw_type == "draw_indexed");

        const int64_t indirectSize = binding.indirect_draw_indexed
            ? (int64_t)sizeof(QRhiIndexedIndirectDrawCommand)
            : (int64_t)sizeof(QRhiIndirectDrawCommand);

        auto* buf = rhi.newBuffer(
            QRhiBuffer::Static,
            QRhiBuffer::StorageBuffer | QRhiBuffer::IndirectBuffer,
            indirectSize);
        qWarning() << "CSF ALLOC [indirectDraw]" << input.name.c_str() << "size=" << indirectSize;
        buf->setName(QByteArray("CSF_IndirectDraw_") + input.name.c_str());
        buf->create();

        // Initialize with zeros (vertexCount=0, instanceCount=0)
        QByteArray zero(indirectSize, 0);
        res.uploadStaticBuffer(buf, 0, indirectSize, zero.constData());

        binding.indirectDrawBuffer = buf;
      }
#endif

      const bool geo_has_output = binding.has_output;
      m_geometryBindings.push_back(std::move(binding));

      if(needs_input)
        input_port_index++;
      if(geo_has_output)
        outlet_index++;

      // $USER ports also create input ports (IntSpinBox), track them
      if(geo->vertex_count.find("$USER") != std::string::npos)
        input_port_index++;
      if(geo->instance_count.find("$USER") != std::string::npos)
        input_port_index++;
      for(const auto& aux : geo->auxiliary)
        if(aux.size.find("$USER") != std::string::npos)
          input_port_index++;
    }
    else
    {
      // All other input types (float, long, bool, event, color, point2D, point3D,
      // image, audio, audioFFT, audioHist, cubemap, texture) create one input port each.
      input_port_index++;
    }
  }

  m_outputTexture = nullptr;

  // Create the compute passes for each output edge (across all output ports)
  for(auto* output_port : n.output)
  {
    for(Edge* edge : output_port->edges)
    {
      const auto& rt = renderer.renderTargetForOutput(*edge);
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

  // Always update geometry bindings when they exist.
  // Unowned buffer pointers reference external GPU buffers whose lifetime
  // we don't control — the upstream node may have freed them since last frame.
  // We must refresh them every frame before recreating SRBs.
  if(!m_geometryBindings.empty())
  {
    updateGeometryBindings(renderer, res);
    this->geometryChanged = false;
  }

  // Recreate SRBs once after all buffer mutations are finalized.
  // This prevents building intermediate SRBs with stale/dangling pointers.
  recreateShaderResourceBindings(renderer, res);

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
      else
      {
        bindingIndex++; // keep indices synchronized with shader layout
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
        else
        {
          bindingIndex++; // keep indices synchronized with shader layout
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

          // "none" access: forwarded via COPY_FROM, no binding needed
          if(req.access == "none")
            continue;

          if(!ssbo.buffer)
          {
            // Create a minimal fallback buffer so we don't skip a binding index
            const int elem_size = glslTypeSizeBytes(req.type);
            ssbo.buffer = rhi.newBuffer(
                QRhiBuffer::Static,
                QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer, elem_size);
            qWarning() << "CSF ALLOC [geomFBFallback]" << req.name.c_str() << "size=" << elem_size;
            ssbo.buffer->setName(QByteArray("CSF_GeomFB_") + req.name.c_str());
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
          else // read_write -> 2 bindings: _in (readonly) + _out (writeonly)
          {
            QRhiBuffer* read_buf = (ssbo.read_buffer && !binding.pending_initial_copy)
                ? ssbo.read_buffer : ssbo.buffer;
            if(read_buf == ssbo.buffer)
            {
              // Same physical buffer for both _in and _out (non-feedback in-place).
              bindings.append(QRhiShaderResourceBinding::bufferLoadStore(
                  bindingIndex++, QRhiShaderResourceBinding::ComputeStage, ssbo.buffer));
              bindings.append(QRhiShaderResourceBinding::bufferLoadStore(
                  bindingIndex++, QRhiShaderResourceBinding::ComputeStage, ssbo.buffer));
            }
            else
            {
              // Distinct buffers (feedback receiver): _in readonly, _out read-write
              bindings.append(QRhiShaderResourceBinding::bufferLoad(
                  bindingIndex++, QRhiShaderResourceBinding::ComputeStage, read_buf));
              bindings.append(QRhiShaderResourceBinding::bufferLoadStore(
                  bindingIndex++, QRhiShaderResourceBinding::ComputeStage, ssbo.buffer));
            }
          }
        }

        // Auxiliary SSBOs for this geometry input
        for(auto& aux : binding.auxiliary_ssbos)
        {
          if(!aux.buffer)
          {
            // Create a minimal fallback buffer so we don't skip a binding index
            aux.buffer = rhi.newBuffer(
                QRhiBuffer::Static, QRhiBuffer::StorageBuffer, 16);
            qWarning() << "CSF ALLOC [auxFBFallback]" << aux.name.c_str() << "size=16";
            aux.buffer->setName(QByteArray("CSF_AuxFB_") + aux.name.c_str());
            aux.buffer->create();
            aux.size = 16;
            aux.owned = true;
          }

          if(aux.access == "read_only")
          {
            bindings.append(QRhiShaderResourceBinding::bufferLoad(
                bindingIndex++, QRhiShaderResourceBinding::ComputeStage, aux.buffer));
          }
          else if(aux.access == "write_only")
          {
            bindings.append(QRhiShaderResourceBinding::bufferStore(
                bindingIndex++, QRhiShaderResourceBinding::ComputeStage, aux.buffer));
          }
          else
          {
            bindings.append(QRhiShaderResourceBinding::bufferLoadStore(
                bindingIndex++, QRhiShaderResourceBinding::ComputeStage, aux.buffer));
          }
        }

#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
        // Rebind indirect draw buffer
        if(binding.uses_indirect_draw && binding.indirectDrawBuffer)
        {
          bindings.append(QRhiShaderResourceBinding::bufferLoadStore(
              bindingIndex++, QRhiShaderResourceBinding::ComputeStage,
              binding.indirectDrawBuffer));
        }
#endif

        geo_binding_index++;
      }
      // Inlet port if any attribute reads from upstream
      for(const auto& attr : geo_input->attributes)
        if(attr.access == "read_only" || attr.access == "read_write") { input_port_index++; break; }
      // Skip $USER ports for this geometry input
      if(geo_input->vertex_count.find("$USER") != std::string::npos) input_port_index++;
      if(geo_input->instance_count.find("$USER") != std::string::npos) input_port_index++;
      for(const auto& aux : geo_input->auxiliary)
        if(aux.size.find("$USER") != std::string::npos) input_port_index++;
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
      qWarning() << "CSF ALLOC [recreateSRB] new SRB for pass";
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
  
  // Clean up pipelines
  for(auto* pip : m_perPassPipelines)
    delete pip;
  m_perPassPipelines.clear();
  m_computePipeline = nullptr;
  
  // Clean up storage buffers
  for(auto& storageBuffer : m_storageBuffers)
  {
    if(storageBuffer.owned)
      r.releaseBuffer(storageBuffer.buffer);
  }
  m_storageBuffers.clear();

  // Clean up GPU scatter
  m_gpuScatter.release();
  m_gpuScatterAvailable = false;

  // Clean up geometry bindings
  for(auto& binding : m_geometryBindings)
  {
    for(auto& ssbo : binding.attribute_ssbos)
    {
      if(ssbo.read_buffer)
      {
        r.releaseBuffer(ssbo.read_buffer);
        ssbo.read_buffer = nullptr;
      }
      if(ssbo.owned && ssbo.buffer)
      {
        r.releaseBuffer(ssbo.buffer);
      }
      ssbo.buffer = nullptr;
      delete ssbo.scatterStaging;
      ssbo.scatterStaging = nullptr;
      delete ssbo.scatterOp.srb;
      ssbo.scatterOp.srb = nullptr;
      delete ssbo.scatterOp.paramsUBO;
      ssbo.scatterOp.paramsUBO = nullptr;
    }
    for(auto& aux : binding.auxiliary_ssbos)
    {
      if(aux.owned && aux.buffer)
      {
        r.releaseBuffer(aux.buffer);
      }
      aux.buffer = nullptr;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    if(binding.indirectDrawBuffer)
    {
      r.releaseBuffer(binding.indirectDrawBuffer);
      binding.indirectDrawBuffer = nullptr;
    }
#endif
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
  m_outStorageImages.clear();
  m_outStorageBuffers.clear();
  m_outputTexture = nullptr;

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
  // Dispatch pending GPU scatter operations (format conversion) before user passes.
  // These convert raw CPU data (e.g. float3) uploaded to staging SSBOs into the
  // format expected by the CSF shader (e.g. vec4), entirely on the GPU.
  {
    // Phase 1: update all scatter params UBOs and SRBs (needs live res batch)
    bool anyScatter = false;
    for(auto& binding : m_geometryBindings)
      for(auto& ssbo : binding.attribute_ssbos)
        if(ssbo.scatterPending)
        {
          anyScatter = true;
          if(res)
            m_gpuScatter.updateParams(*res, ssbo.scatterOp, ssbo.scatterParams);
        }

    // Phase 2: dispatch all scatters inside a single compute pass
    if(anyScatter)
    {
      commands.beginComputePass(res, QRhiCommandBuffer::BeginPassFlag::ExternalContent);
      res = nullptr;

      for(auto& binding : m_geometryBindings)
      {
        for(auto& ssbo : binding.attribute_ssbos)
        {
          if(ssbo.scatterPending)
          {
            m_gpuScatter.dispatch(commands, ssbo.scatterOp, ssbo.scatterParams);

            // Barrier so writes are visible to subsequent dispatches
            commands.beginExternal();
            insertComputeBarrier(*renderer.state.rhi, commands);
            commands.endExternal();

            ssbo.scatterPending = false;
          }
        }
      }

      commands.endComputePass();
      res = renderer.state.rhi->nextResourceUpdateBatch();
    }
  }

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

    // Begin compute pass with ExternalContent flag so we can insert
    // native memory barriers between dispatches via beginExternal/endExternal.
    commands.beginComputePass(res, QRhiCommandBuffer::BeginPassFlag::ExternalContent);
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

    // Resolve per-axis stride expressions
    const int strideX = resolveDispatchExpression(passDesc.stride[0]);
    const int strideY = resolveDispatchExpression(passDesc.stride[1]);
    const int strideZ = resolveDispatchExpression(passDesc.stride[2]);

    // Calculate dispatch size based on execution model
    if(passDesc.execution_type == "2D_IMAGE")
    {
      // For 2D image execution, dispatch based on image size, workgroup size and stride
      QSize textureSize = m_outputTexture ? m_outputTexture->pixelSize() : QSize(1280, 720);
      dispatchX = (textureSize.width() + localX * strideX - 1) / (localX * strideX);
      dispatchY = (textureSize.height() + localY * strideY - 1) / (localY * strideY);
      dispatchZ = 1;
    }
    else if(passDesc.execution_type == "3D_IMAGE")
    {
      // For 3D image execution, dispatch based on volume dimensions and strides
      if(m_outputTexture)
      {
        QSize sz = m_outputTexture->pixelSize();
        int depth = m_outputTexture->depth();
        dispatchX = (sz.width() + localX * strideX - 1) / (localX * strideX);
        dispatchY = (sz.height() + localY * strideY - 1) / (localY * strideY);
        dispatchZ = (depth + localZ * strideZ - 1) / (localZ * strideZ);
      }
      else
      {
        dispatchX = 1;
        dispatchY = 1;
        dispatchZ = 1;
      }
    }
    else if(passDesc.execution_type == "MANUAL")
    {
      // For manual execution, use specified workgroups
      dispatchX = passDesc.workgroups[0];
      dispatchY = passDesc.workgroups[1];
      dispatchZ = passDesc.workgroups[2];
    }
    else if(passDesc.execution_type == "USER")
    {
      // For user-controlled execution, read dispatch counts from UI ports
      int* dispatch[3] = {&dispatchX, &dispatchY, &dispatchZ};
      for(int axis = 0; axis < 3; axis++)
      {
        int port_idx = passDesc.user_dispatch_ports[axis];
        if(port_idx >= 0 && port_idx < (int)n.input.size())
        {
          auto port = n.input[port_idx];
          *dispatch[axis] = (port && port->value) ? std::max(1, *(int*)port->value) : 1;
        }
        else
        {
          *dispatch[axis] = 1;
        }
      }
    }
    else if(
        passDesc.execution_type == "1D_BUFFER"
        || passDesc.execution_type == "PER_VERTEX"
        || passDesc.execution_type == "PER_INSTANCE")
    {
      int n = 1;

      if(passDesc.execution_type == "PER_VERTEX")
      {
        // Dispatch one thread per vertex in the target geometry
        for(const auto& geo_bind : m_geometryBindings)
        {
          if(geo_bind.vertex_count > 0)
          {
            n = geo_bind.vertex_count;
            break;
          }
        }
      }
      else if(passDesc.execution_type == "PER_INSTANCE")
      {
        // Dispatch one thread per instance in the target geometry
        for(const auto& geo_bind : m_geometryBindings)
        {
          if(geo_bind.instance_count > 0)
          {
            n = geo_bind.instance_count;
            break;
          }
        }
      }
      else
      {
        // 1D_BUFFER: try storage buffer size first, then geometry element count
        for(auto& [port, index] : this->m_outStorageBuffers) {
          if(port == edge.source) {
            n = this->m_storageBuffers[index].size;
            break;
          }
        }

        if(n <= 1)
        {
          for(const auto& geo_bind : m_geometryBindings)
          {
            if(geo_bind.vertex_count > 0)
            {
              n = geo_bind.vertex_count;
              break;
            }
          }
        }
      }

      const auto requiredInvocations = n;
      const auto threadsPerWorkgroup = localX * localY * localZ;
      const int64_t totalWorkgroups = (requiredInvocations + threadsPerWorkgroup * strideX - 1)
                                      / (threadsPerWorkgroup * strideX);
      qDebug() << "CSF dispatch: pass" << passIndex
               << "type=" << passDesc.execution_type.c_str() << "n=" << n
               << "local=" << threadsPerWorkgroup << "workgroups=" << totalWorkgroups;
      // Log SSBO sizes on first pass only
      if(passIndex == 0)
      {
        for(int gi = 0; gi < (int)m_geometryBindings.size(); gi++)
        {
          const auto& gb = m_geometryBindings[gi];
          for(int ai = 0; ai < (int)gb.attribute_ssbos.size(); ai++)
          {
            const auto& s = gb.attribute_ssbos[ai];
            if(s.buffer)
              qDebug() << "  geo" << gi << "ssbo" << ai << s.name.c_str()
                       << "size=" << s.size << "access=" << s.access.c_str();
          }
        }
      }
      static constexpr int64_t maxWorkgroups = 65535;

      if(totalWorkgroups > maxWorkgroups * maxWorkgroups * maxWorkgroups)
      {
        commands.endComputePass();
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
      // Default fallback (same as 2D_IMAGE with strides)
      QSize textureSize = m_outputTexture ? m_outputTexture->pixelSize() : QSize(1280, 720);
      dispatchX = (textureSize.width() + localX * strideX - 1) / (localX * strideX);
      dispatchY = (textureSize.height() + localY * strideY - 1) / (localY * strideY);
      dispatchZ = 1;
    }

    // Guard against dispatch(0,0,0) which is invalid per Vulkan spec
    if(dispatchX <= 0 || dispatchY <= 0 || dispatchZ <= 0)
    {
      commands.endComputePass();
      continue;
    }

    // Dispatch compute shader
    commands.dispatch(dispatchX, dispatchY, dispatchZ);

    // End compute pass

    // Insert a compute→compute memory barrier so that SSBO writes from
    // this dispatch are visible to the next dispatch. QRhi does not
    // insert these automatically between consecutive compute passes.
    commands.beginExternal();
    insertComputeBarrier(*renderer.state.rhi, commands);
    commands.endExternal();

    commands.endComputePass();
  }

  // After all compute passes: push output geometry to downstream nodes
  if(!m_geometryBindings.empty())
  {
    if(!res)
      res = renderer.state.rhi->nextResourceUpdateBatch();
    pushOutputGeometry(renderer, *res, edge);
  }

  // Ping-pong swap for feedback receivers: after pushing output,
  // swap so that next frame reads from what was just written.
  // Also clear pending_initial_copy: on the first frame, same-buffer mode
  // was used (_in == _out == buffer) so init+simulate worked in-place.
  // The swap puts that buffer into read_buffer, seeding the ping-pong
  // for frame 2+ without needing an explicit GPU copy.
  {
    int gb_idx = 0;
    for(const auto& input : n.m_descriptor.inputs)
    {
      auto* geo_input = ossia::get_if<isf::geometry_input>(&input.data);
      if(!geo_input)
        continue;
      if(gb_idx >= (int)m_geometryBindings.size())
        break;
      auto& gb = m_geometryBindings[gb_idx];
      if(gb.is_feedback_receiver)
      {
        gb.pending_initial_copy = false;
        for(int ai = 0; ai < (int)geo_input->attributes.size(); ai++)
        {
          if(ai >= (int)gb.attribute_ssbos.size())
            break;
          auto& ssbo = gb.attribute_ssbos[ai];
          if(geo_input->attributes[ai].access == "read_write" && ssbo.read_buffer)
            std::swap(ssbo.buffer, ssbo.read_buffer);
        }
      }
      gb_idx++;
    }
  }
}
}
