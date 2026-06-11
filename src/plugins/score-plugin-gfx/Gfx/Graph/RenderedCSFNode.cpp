#include "ossia/detail/fmt.hpp"

#include <Gfx/Graph/CommonUBOs.hpp>
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/ISFVisitors.hpp>
#include <Gfx/Graph/IsfBindingsBuilder.hpp>
#include <Gfx/Graph/RenderedCSFNode.hpp>
#include <Gfx/Graph/RenderedISFSamplerUtils.hpp>
#include <Gfx/Graph/RhiComputeBarrier.hpp>
#include <Gfx/Graph/SSBO.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/dataflow/geometry_port.hpp>
#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/hash.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/math/math_expression.hpp>

#include <boost/algorithm/string/replace.hpp>

#include <unordered_map>
#include <unordered_set>

namespace score::gfx
{
static QRhiTexture::Format
getTextureFormat(const QString& format)  noexcept
{
  // Map CSF format strings to Qt RHI texture formats.
  //
  // Case-insensitive comparison: libisf emits the FORMAT layout qualifier
  // lowercased into the GLSL (`layout(r32ui) uniform uimage3D ...`), but
  // the CSF JSON parser stores `image->format` verbatim — so an author
  // writing `"FORMAT": "r32ui"` (the lowercase form that matches the
  // generated GLSL one-to-one) used to silently fall through to the
  // RGBA8 default at texture creation, while the shader compiled with
  // r32ui — producing a Vulkan validation error
  // VUID-vkCmdDispatch-format-07753 ("UINT component type required, bound
  // descriptor format is VK_FORMAT_R8G8B8A8_UNORM") and undefined values
  // on every imageLoad / imageStore. Normalise to upper-case once and
  // dispatch.
  const QString f = format.toUpper();

  if(f == "RGBA8")    return QRhiTexture::RGBA8;
  if(f == "BGRA8")    return QRhiTexture::BGRA8;
  if(f == "R8")       return QRhiTexture::R8;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  if(f == "RG8")      return QRhiTexture::RG8;
#endif
  if(f == "R16")      return QRhiTexture::R16;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  if(f == "RG16")     return QRhiTexture::RG16;
#endif
  if(f == "RGBA16F")  return QRhiTexture::RGBA16F;
  if(f == "RGBA32F")  return QRhiTexture::RGBA32F;
  if(f == "R16F")     return QRhiTexture::R16F;
  if(f == "R32F")     return QRhiTexture::R32F;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  if(f == "RGB10A2")  return QRhiTexture::RGB10A2;
#endif

  // Integer formats — required for atomic image ops (imageAtomicOr / Add /
  // Min / Max / Exchange / CompareExchange in GLSL). Atomics in Vulkan,
  // D3D12 and Metal 3.1+ work on the R{8,32}{UI,SI} family; the wider
  // {RG,RGBA}{32}{UI,SI} variants are sample-only on most desktop GPUs but
  // still legal as storage images. Keep symmetry with QRhiTexture::Format
  // — RG32UI / RGBA32UI are exposed so users who want to pack two/four
  // counters per voxel into one atomic-OR target can opt in.
  //
  // Added to QRhiTexture::Format in Qt 6.10 — guard so older Qt builds
  // (6.2 / 6.4 / 6.6 / 6.8) compile. On older Qt, the request silently
  // falls through to RGBA8 (and a Vulkan validation error if the shader
  // declared an integer layout qualifier on its image), but the builds
  // don't break.
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
  if(f == "R8UI")     return QRhiTexture::R8UI;
  if(f == "R32UI")    return QRhiTexture::R32UI;
  if(f == "RG32UI")   return QRhiTexture::RG32UI;
  if(f == "RGBA32UI") return QRhiTexture::RGBA32UI;
  if(f == "R8SI")     return QRhiTexture::R8SI;
  if(f == "R32SI")    return QRhiTexture::R32SI;
  if(f == "RG32SI")   return QRhiTexture::RG32SI;
  if(f == "RGBA32SI") return QRhiTexture::RGBA32SI;
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

void RenderedCSFNode::updateInputTexture(const Port& input, QRhiTexture* tex, QRhiTexture* depthTex)
{
  int sampler_idx = 0;
  for(auto* p : node.input)
  {
    if(p == &input)
      break;
    if(p->type == Types::Image)
    {
      sampler_idx++;
      if((p->flags & Flag::SamplableDepth) == Flag::SamplableDepth)
        sampler_idx++;
    }
  }

  auto replaceSampler = [&](Sampler& sampl, QRhiTexture* t)
  {
    if(sampl.texture != t)
    {
      sampl.texture = t;
      for(auto& [e, cp] : m_computePasses)
        if(cp.srb)
          score::gfx::replaceTexture(*cp.srb, sampl.sampler, t);
      for(auto& [e, gp] : m_graphicsPasses)
        if(gp.pipeline.srb)
          score::gfx::replaceTexture(*gp.pipeline.srb, sampl.sampler, t);
    }
  };

  if(sampler_idx < (int)m_inputSamplers.size())
  {
    replaceSampler(m_inputSamplers[sampler_idx], tex);

    if(depthTex
       && (input.flags & Flag::SamplableDepth) == Flag::SamplableDepth
       && sampler_idx + 1 < (int)m_inputSamplers.size())
    {
      replaceSampler(m_inputSamplers[sampler_idx + 1], depthTex);
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

// Thin adapter over the canonical isf_input_port_count_vis (ISFVisitors.hpp) so
// the existing call sites that do `ossia::visit(p, input.data)` keep working.
// Use walk_descriptor_inputs() in new code; this shim preserves the
// "inlet_i / outlet_i mid-loop" pattern these consumers rely on.
struct port_indices
{
  int inlet_i = 0;
  int outlet_i = 0;
  template <typename T>
  void operator()(const T& v) noexcept
  {
    auto c = isf_input_port_count_vis{}(v);
    inlet_i += c.inlets;
    outlet_i += c.outlets;
  }
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

  // Try fixed integer first — but only when the whole string is a pure
  // integer literal. std::stoi greedily parses the leading digits and
  // silently stops at the first non-digit, so "6 * $x * $x" would
  // otherwise be accepted as the integer 6 and the expression evaluator
  // never runs. Require every character after optional leading whitespace
  // to be a digit before taking the fast path.
  {
    std::size_t i = 0;
    while(i < expr.size() && std::isspace((unsigned char)expr[i]))
      ++i;
    const std::size_t first_digit = i;
    while(i < expr.size() && std::isdigit((unsigned char)expr[i]))
      ++i;
    std::size_t last_digit = i;
    while(i < expr.size() && std::isspace((unsigned char)expr[i]))
      ++i;
    if(first_digit < last_digit && i == expr.size())
    {
      try
      {
        return std::max(1, std::stoi(expr));
      }
      catch(...)
      {
      }
    }
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

  // Register full geometry of each input image/texture:
  //   $WIDTH_<name>, $HEIGHT_<name>, $DEPTH_<name>, $LAYERS_<name>
  //
  // DEPTH/LAYERS are sourced from the live QRhiTexture when available
  // (tex->depth() for 3D, tex->arraySize() for arrays). Both fall back to 1
  // for plain 2D textures so expressions like "$DEPTH_vol" remain defined
  // regardless of whether the bound texture is actually volumetric — lets
  // shaders write one size formula and have it parse cleanly in both cases.
  //
  // The first input image also exposes un-suffixed $WIDTH/$HEIGHT/$DEPTH/
  // $LAYERS for the common "filter that inherits its input's size" case.
  auto register_texture_size = [&](const std::string& name, QRhiTexture* tex,
                                   bool& first) {
    QSize px = tex ? tex->pixelSize() : QSize{1280, 720};
    int depth = 1;
    int layers = 1;
    if(tex)
    {
      if((int)(tex->flags() & QRhiTexture::ThreeDimensional))
        depth = std::max(1, tex->depth());
      if((int)(tex->flags() & QRhiTexture::TextureArray))
        layers = std::max(1, tex->arraySize());
    }
    if(px.width() <= 0)
      px.setWidth(1280);
    if(px.height() <= 0)
      px.setHeight(720);

    e.add_constant(fmt::format("var_WIDTH_{}", name), data.emplace_back(px.width()));
    e.add_constant(fmt::format("var_HEIGHT_{}", name), data.emplace_back(px.height()));
    e.add_constant(fmt::format("var_DEPTH_{}", name), data.emplace_back(depth));
    e.add_constant(fmt::format("var_LAYERS_{}", name), data.emplace_back(layers));
    if(first)
    {
      e.add_constant("var_WIDTH", data.emplace_back(px.width()));
      e.add_constant("var_HEIGHT", data.emplace_back(px.height()));
      e.add_constant("var_DEPTH", data.emplace_back(depth));
      e.add_constant("var_LAYERS", data.emplace_back(layers));
      first = false;
    }
  };

  bool first_image = true;
  int input_image_index = 0;
  for(const auto& img : desc.inputs)
  {
    if(ossia::get_if<isf::texture_input>(&img.data))
    {
      QRhiTexture* t = nullptr;
      if(input_image_index < (int)m_inputSamplers.size())
        t = this->m_inputSamplers[input_image_index].texture;
      register_texture_size(img.name, t, first_image);
      input_image_index++;
    }
    else if(auto* img_input = ossia::get_if<isf::csf_image_input>(&img.data))
    {
      // Resolve dimensions for ALL csf_image_input access modes:
      //   - read_only: bound as sampled texture in m_inputSamplers
      //   - write_only / read_write: bound as storage image in m_storageImages
      QRhiTexture* t = nullptr;
      if(img_input->access == "read_only")
      {
        if(input_image_index < (int)m_inputSamplers.size())
          t = this->m_inputSamplers[input_image_index].texture;
        input_image_index++;
      }
      else
      {
        auto it = std::find_if(
            m_storageImages.begin(), m_storageImages.end(),
            [&](const StorageImage& si) { return si.name.toStdString() == img.name; });
        if(it != m_storageImages.end())
          t = it->texture;
      }
      register_texture_size(img.name, t, first_image);
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
  //
  // Always register the symbol so the expression parses, even on the very
  // first frame when no upstream geometry has flowed yet — fall back to the
  // descriptor's static vertex_count/instance_count strings (parsed as int)
  // and ultimately to 1. Without this fallback, $VERTEX_COUNT_<name> raises
  // ERR232 - Undefined symbol on every dispatch evaluation that runs before
  // updateGeometryBindings has populated geo_bind, breaking csf-copy-from /
  // csf-geo-read-write and any CSF whose dispatch refers to a not-yet-bound
  // geometry input.
  auto parse_static_count = [](const std::string& s, int fallback) -> int {
    if(s.empty()) return fallback;
    try
    {
      int v = std::stoi(s);
      return v > 0 ? v : fallback;
    }
    catch(...)
    {
      return fallback;
    }
  };

  int geo_idx = 0;
  bool first_geo = true;
  for(const auto& input : desc.inputs)
  {
    if(auto* geo = ossia::get_if<isf::geometry_input>(&input.data))
    {
      int vertex_count = 0;
      int instance_count = 0;
      if(geo_idx < (int)m_geometryBindings.size())
      {
        const auto& geo_bind = m_geometryBindings[geo_idx];
        vertex_count = geo_bind.vertex_count;
        instance_count = geo_bind.instance_count;
      }
      if(vertex_count <= 0)
        vertex_count = parse_static_count(geo->vertex_count, 1);
      if(instance_count <= 0)
        instance_count = parse_static_count(geo->instance_count, 1);

      e.add_constant(
          fmt::format("var_VERTEX_COUNT_{}", input.name),
          data.emplace_back(vertex_count));
      e.add_constant(
          fmt::format("var_INSTANCE_COUNT_{}", input.name),
          data.emplace_back(instance_count));
      if(first_geo)
      {
        e.add_constant("var_VERTEX_COUNT", data.emplace_back(vertex_count));
        e.add_constant("var_INSTANCE_COUNT", data.emplace_back(instance_count));
        first_geo = false;
      }
      geo_idx++;
    }
  }

  // Register $COUNT_<name> and $BYTESIZE_<name> for every addressable SSBO /
  // UBO the node binds, input or output. Lets SIZE / TARGET / WIDTH / HEIGHT
  // expressions size themselves to upstream buffer extents by name —
  // removes the need for user-visible "max N" scalar inputs on filters
  // whose output should always mirror their input size.
  //
  // Registration order matters when names collide (e.g. an upstream-
  // provided nested aux shadowed by a top-level AUXILIARY of the same
  // name in a replace-style shader): the nested (input-side) binding
  // is registered first so the top-level (output-side) redundant
  // re-registration is suppressed — semantically, when a user writes
  // `$COUNT_scene_lights` they mean the upstream count, not the size
  // of the output buffer they're about to overwrite.
  //
  // For UBOs, COUNT always resolves to 1 (a UBO is one struct instance);
  // BYTESIZE resolves to the struct byte size. For SSBOs with a flexible
  // array, stride is inferred from `calculateStorageBufferSize(layout, 1)
  // - calculateStorageBufferSize(layout, 0)` and COUNT is the allocation's
  // element count. For SSBOs without a flexible array, COUNT resolves to 1.
  {
    ossia::hash_set<std::string> registered;
    const auto& eff_desc = n.descriptor();

    auto register_buffer
        = [&](const std::string& name, int64_t byte_size, bool is_uniform,
              std::span<const isf::storage_input::layout_field> layout) {
      if(name.empty() || registered.contains(name))
        return;
      int64_t element_count = 1;
      if(is_uniform)
      {
        // UBO: single struct. $COUNT = 1, $BYTESIZE = struct size.
        element_count = 1;
      }
      else
      {
        const int64_t fixed_part
            = score::gfx::calculateStorageBufferSize(layout, 0, eff_desc);
        const int64_t with_one
            = score::gfx::calculateStorageBufferSize(layout, 1, eff_desc);
        const int64_t stride = with_one - fixed_part;
        if(stride > 0 && byte_size > fixed_part)
          element_count = (byte_size - fixed_part) / stride;
        else
          element_count = 1;
        if(element_count < 1)
          element_count = 1;
      }
      e.add_constant(
          fmt::format("var_COUNT_{}", name),
          data.emplace_back((double)element_count));
      e.add_constant(
          fmt::format("var_BYTESIZE_{}", name),
          data.emplace_back((double)byte_size));
      registered.insert(name);
    };

    // Pass 1 — nested auxiliaries on every geometry input (the "upstream
    // side" of filters; these are the buffers whose counts the user most
    // often wants to size against). Registered first so collisions with
    // top-level same-name overrides in Pass 2 fall through.
    for(const auto& binding : m_geometryBindings)
    {
      for(const auto& aux : binding.auxiliary_ssbos)
      {
        register_buffer(aux.name, aux.size, aux.is_uniform, aux.layout);
      }
    }

    // Pass 2 — top-level storage buffers (INPUTS storage_input +
    // top-level AUXILIARY writes).
    for(const auto& sb : m_storageBuffers)
    {
      // Whether this top-level buffer is a UBO or SSBO depends on the
      // descriptor input it came from; look up by name.
      bool is_uniform = false;
      for(const auto& inp : eff_desc.inputs)
      {
        if(inp.name == sb.name.toStdString())
        {
          if(ossia::get_if<isf::uniform_input>(&inp.data))
            is_uniform = true;
          break;
        }
      }
      register_buffer(sb.name.toStdString(), sb.size, is_uniform, sb.layout);
    }
  }
}

int RenderedCSFNode::resolveDispatchExpression(const std::string& expr) const
{
  if(expr.empty())
    return 1;

  // Pure integer literal fast-path. Same guard as resolveCountExpression:
  // std::stoi would otherwise silently accept "6 * $x" as 6.
  {
    std::size_t i = 0;
    while(i < expr.size() && std::isspace((unsigned char)expr[i]))
      ++i;
    const std::size_t first_digit = i;
    while(i < expr.size() && std::isdigit((unsigned char)expr[i]))
      ++i;
    std::size_t last_digit = i;
    while(i < expr.size() && std::isspace((unsigned char)expr[i]))
      ++i;
    if(first_digit < last_digit && i == expr.size())
    {
      try
      {
        return std::max(1, std::stoi(expr));
      }
      catch(...)
      {
      }
    }
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
  qWarning() << "RenderedCSFNode: storage size port not resolved (storageSizeInputIndex="
             << storageSizeInputIndex << "); falling back to 1024.";
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
    for(const auto& [port_key, geo_spec] : m_portGeometries)
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

// GLSL type → byte size lives in IsfBindingsBuilder.hpp
// (score::gfx::glslTypeSizeBytes for the bare type, std430ArrayStride for
// the per-element stride inside an std430 SSBO array — these differ for
// vec3, see header doc for the rationale). All call sites below resolve
// via ADL inside `namespace score::gfx`.

// Returns the byte size of one upstream-side element of an
// ossia::geometry attribute. For the user_struct format the producer
// carries the size out-of-line on `element_byte_size` (sizeof of the
// user-defined struct in std430); otherwise dispatches on the format
// enum.
static int geometryFormatSizeBytes(const ossia::geometry::attribute& a) noexcept
{
  using F = ossia::geometry::attribute;
  switch(a.format)
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
    case F::user_struct: return (int)a.element_byte_size;
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
        if(auto* geo = findGeometryByPort(binding.input_port_index);
           geo && geo->meshes && !geo->meshes->meshes.empty())
        {
          binding.vertex_count = geo->meshes->meshes[0].vertices;
          if(geo->meshes->meshes[0].instances > 0)
            binding.instance_count = geo->meshes->meshes[0].instances;
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
      if(auto* geo = findGeometryByPort(binding.input_port_index);
         geo && geo->meshes && !geo->meshes->meshes.empty())
      {
        binding_has_upstream = true;
        upstream_mesh = &geo->meshes->meshes[0];
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
          buf->setName(QByteArray("CSF_GeoAux_") + aux.name.c_str());
          buf->create();
          aux.buffer = buf;
          aux.owned = true;
        }
        QByteArray zero(requiredSize, 0);
        res.uploadStaticBuffer(aux.buffer, 0, requiredSize, zero.constData());
        aux.size = requiredSize;

        // Keep read_buffer in sync for feedback receivers
        if(aux.read_buffer)
        {
          aux.read_buffer->destroy();
          aux.read_buffer->setSize(requiredSize);
          aux.read_buffer->create();
          res.uploadStaticBuffer(aux.read_buffer, 0, requiredSize, zero.constData());
        }
      }
    }

    // Detect feedback receiver: a GENERATOR (has vertex_count_spec) that receives
    // upstream geometry ON ITS OWN PORT. Only then is it a feedback loop.
    // Bindings whose port receives external data (from a different node) should
    // NOT be marked as feedback receivers.
    if(binding.has_vertex_count_spec && binding_has_upstream && !binding.is_feedback_receiver)
    {
      // Heuristic: check if the upstream buffer pointers match our OWN (owned)
      // SSBOs. If they do, this is genuine feedback (our own output looped back).
      // If the matching buffer is merely ADOPTED from upstream (ssbo.owned==false),
      // this is a linear chain where last frame's adoption left ssbo.buffer pointing
      // at the upstream node's buffer — that must NOT be treated as self-feedback.
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
                if(ssbo.owned
                   && ssbo.buffer == static_cast<QRhiBuffer*>(gpu->handle))
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
            const int64_t elem_stride = std430ArrayStride(req.type, n.m_descriptor);
            const int count = ssbo.per_instance ? binding.instance_count : binding.vertex_count;
            const int64_t buf_size = elem_stride * count;
            if(buf_size > 0)
            {
              auto* buf = renderer.state.rhi->newBuffer(
                  QRhiBuffer::Static,
                  QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer, buf_size);
              buf->setName(QByteArray("CSF_GeomPP_") + req.name.c_str());
              buf->create();
              QByteArray zero(buf_size, 0);
              res.uploadStaticBuffer(buf, 0, buf_size, zero.constData());
              ssbo.read_buffer = buf;
            }
          }
        }

        // Allocate ping-pong read_buffers for read_write auxiliary SSBOs.
        // Mirrors the attribute path above so that aux state (e.g. uint counters,
        // free lists) can persist across feedback frames the same way attributes do.
        for(auto& aux : binding.auxiliary_ssbos)
        {
          if(aux.access == "read_write" && !aux.read_buffer && aux.size > 0)
          {
            auto* buf = renderer.state.rhi->newBuffer(
                QRhiBuffer::Static,
                QRhiBuffer::StorageBuffer, aux.size);
            buf->setName(QByteArray("CSF_GeomPPAux_") + aux.name.c_str());
            buf->create();
            QByteArray zero(aux.size, 0);
            res.uploadStaticBuffer(buf, 0, aux.size, zero.constData());
            aux.read_buffer = buf;
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

        // Match against upstream geometry — same 3-stage cascade as raw
        // raster (findGeometryAttribute in Utils.cpp). The display_name
        // stage handles `{ NAME: "position", SEMANTIC: "custom" }` falling
        // back to the real position attribute when no shadowing custom one
        // exists.
        const ossia::geometry::attribute* geo_attr
            = score::gfx::findGeometryAttribute(mesh, req.name, req.semantic);

        if(!geo_attr)
        {
          // Create or keep a zero-filled fallback buffer. std430ArrayStride
          // ensures vec3 attributes get 16-byte stride to match what the
          // shader's `T array[]` SSBO actually reads in std430.
          const int64_t elem_stride = std430ArrayStride(req.type, n.m_descriptor);
          const int fallback_count = ssbo.per_instance ? std::max(1, mesh.instances) : std::max(1, mesh.vertices);
          const int64_t needed = elem_stride * fallback_count;
          if(!ssbo.buffer || ssbo.size < needed)
          {
            if(req.required && req.access == "read_only")
              qWarning() << "CSF geometry: required read_only attribute"
                         << req.name.c_str() << "not found"
                         << "(semantic=" << req.semantic.c_str() << ")";
            else
              qDebug() << "  attr" << req.name.c_str() << "not in upstream — creating fallback buffer";

            if(ssbo.buffer && ssbo.owned)
            {
              renderer.releaseBuffer(ssbo.buffer);
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

            // Keep read_buffer in sync for feedback receivers
            if(ssbo.read_buffer)
            {
              ssbo.read_buffer->destroy();
              ssbo.read_buffer->setSize(needed);
              ssbo.read_buffer->create();
              res.uploadStaticBuffer(ssbo.read_buffer, 0, needed, zero.constData());
            }
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

        const int attr_size = geometryFormatSizeBytes(*geo_attr);
        const int64_t csf_elem_stride = std430ArrayStride(req.type, n.m_descriptor);
        const int stride = geo_bind.byte_stride;
        // SoA upstream is bindable directly when the binding stride matches
        // either the bare attribute size (tightly-packed mesh vertex buffer)
        // or the std430 element stride (CSF SSBO output, vec3-padded). Both
        // shapes are valid sources for an std430 SSBO consumer.
        const bool is_soa = (stride == 0 || stride == attr_size
                             || stride == (int)csf_elem_stride);

        if(auto* gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&geo_buf.data))
        {
          const int elem_size = glslTypeSizeBytes(req.type, n.m_descriptor);
          if(is_soa && gpu->handle
             && (attr_size == elem_size || stride == (int)csf_elem_stride))
          {
            // SoA GPU buffer with matching element size: bind directly (zero-copy)
            auto* rhi_buf = static_cast<QRhiBuffer*>(gpu->handle);

            // If this node has a vertex_count_spec ($USER), its own SSBOs are
            // authoritative. Don't replace a properly-sized owned buffer with
            // an undersized upstream one (happens on the first frame of a
            // feedback loop when the downstream node hasn't produced data yet).
            if(binding.has_vertex_count_spec && ssbo.owned && ssbo.buffer)
            {
              const int attr_count = ssbo.per_instance ? binding.instance_count : binding.vertex_count;
              const int64_t needed = csf_elem_stride * attr_count;
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
          const int64_t elem_size = glslTypeSizeBytes(req.type, n.m_descriptor);
          const int64_t elem_stride = std430ArrayStride(req.type, n.m_descriptor);
          const int data_count = ssbo.per_instance ? mesh.instances : mesh.vertices;
          const int64_t needed = elem_stride * data_count;

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
            buf->setName(QByteArray("CSF_Geom_") + req.name.c_str());
            buf->create();
            ssbo.buffer = buf;
            ssbo.size = needed;
            ssbo.owned = true;

            // For feedback receivers, also resize read_buffer to keep both
            // ping-pong buffers the same size. Otherwise after the swap,
            // ssbo.buffer would be the old undersized read_buffer while
            // ssbo.size reflects the new size, causing buffer overruns.
            if(ssbo.read_buffer)
            {
              ssbo.read_buffer->destroy();
              ssbo.read_buffer->setSize(needed);
              ssbo.read_buffer->create();
            }
          }

          // Total byte offset into the buffer: input entry offset + attribute offset within stride
          const int64_t base_offset = input_byte_offset + geo_attr->byte_offset;

          // Direct upload only when source and destination strides match
          // exactly. For vec3 attributes, that means upstream must already
          // be std430-strided (16 bytes per element) — a tightly-packed
          // upstream vec3 (stride 12) routes through scatter so the
          // destination's 4-byte trailing padding stays zeroed.
          if(is_soa && (int64_t)stride == elem_stride)
          {
            // SoA CPU buffer with matching stride: upload directly
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

            // The scatter compute lays out destination elements at
            // dst_components * sizeof(float) per slot — for vec3 in std430
            // that's 3 floats of data + 1 float of padding implicit in the
            // 16-byte stride. dst_components is 3 for vec3, so the compute
            // writes 12 bytes per element and the buffer's std430 padding
            // bytes stay at their zero-initialised value. That matches
            // what a well-behaved compute shader would produce.
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
            // (used when compute shaders are not available). Destination
            // slots are elem_stride bytes apart; the first elem_size bytes
            // hold the data, any trailing std430 padding stays zero.
            QByteArray scattered(needed, 0);

            if(elem_size > attr_size && elem_size >= (int)sizeof(float))
            {
              const float one = 1.0f;
              for(int i = 0; i < data_count; i++)
                std::memcpy(scattered.data() + (int64_t)i * elem_stride + elem_size - sizeof(float),
                            &one, sizeof(float));
            }

            const int copy_size = std::min((int)elem_size, attr_size);
            for(int i = 0; i < data_count; i++)
            {
              const int64_t src_off = (int64_t)i * stride + base_offset;
              if(src_off + copy_size <= cpu->byte_size)
                std::memcpy(scattered.data() + (int64_t)i * elem_stride, src + src_off, copy_size);
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
            // Usage flag matches the aux kind so the created buffer can
            // be bound as the intended descriptor type.
            const auto usage = aux.is_uniform ? QRhiBuffer::UniformBuffer
                                              : QRhiBuffer::StorageBuffer;
            auto* buf = renderer.state.rhi->newBuffer(
                QRhiBuffer::Static, usage, requiredSize);
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

      // Auxiliary textures: match by name against the mesh's
      // auxiliary_textures list. Fall back to the shape-matched
      // placeholder when no match — same safety model as the raster
      // path (never leave a stale upstream handle that may have been
      // freed). SRB rebuild on handle change is driven by the existing
      // initComputeSRBAndPasses / recreateSRB cycle; we only update
      // the cached texture pointer here.
      for(auto& at : binding.auxiliary_textures)
      {
        // Owned textures (auto-allocated writable storage images) are
        // never overwritten by upstream resolution — we own the data,
        // there's no upstream contributor.
        if(at.owned)
          continue;
        const auto* aux = mesh.find_auxiliary_texture(at.name);
        auto* tex = aux
            ? static_cast<QRhiTexture*>(aux->native_handle)
            : nullptr;
        if(!tex)
          tex = at.placeholder;
        at.texture = tex;
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
          const int64_t elem_stride = std430ArrayStride(geo_input->attributes[attr_idx].type, n.m_descriptor);
          const int attr_count = ssbo.per_instance ? binding.instance_count : binding.vertex_count;
          const int64_t needed = elem_stride * attr_count;
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
          const int64_t elem_stride = std430ArrayStride(req.type, n.m_descriptor);
          const int64_t needed = elem_stride * count;

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
      if(auto* geo = findGeometryByPort(binding.input_port_index);
         geo && geo->meshes && !geo->meshes->meshes.empty())
      {
        binding_upstream = &geo->meshes->meshes[0];
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
      // Release previous COPY_FROM buffers (they escape into output geometry)
      for(auto* buf : binding.copyFromBuffers)
        renderer.releaseBuffer(buf);
      binding.copyFromBuffers.clear();

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
        // Inherit topology / cull / face / blend / depth-write / filter
        // metadata from upstream for filter-type nodes. Anything the CSF
        // doesn't explicitly produce on its own should pass through —
        // otherwise inserting a CSF between ScenePreprocessor and a
        // rasterizer silently drops state the rasterizer relies on.
        out_geo.topology   = (decltype(out_geo.topology))binding_upstream->topology;
        out_geo.cull_mode  = (decltype(out_geo.cull_mode))binding_upstream->cull_mode;
        out_geo.front_face = (decltype(out_geo.front_face))binding_upstream->front_face;
        out_geo.blend       = binding_upstream->blend;
        out_geo.depth_write = binding_upstream->depth_write;
        out_geo.filter_tag            = binding_upstream->filter_tag;
        out_geo.filter_material_index = binding_upstream->filter_material_index;
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
        // The buffer underneath is sized at std430 stride (16 bytes per
        // vec3 element); declaring the binding stride to match is what
        // lets a downstream raw-raster vertex shader read these
        // attributes without the silent vec3-padding drift that left
        // every fourth splat misaligned.
        const int64_t elem_stride = std430ArrayStride(req.type, n.m_descriptor);

        ossia::geometry::buffer buf{
            .data = ossia::geometry::gpu_buffer{ssbo.buffer, ssbo.size},
            .dirty = false};
        out_geo.buffers.push_back(std::move(buf));

        ossia::geometry::binding bind;
        bind.byte_stride = (uint32_t)elem_stride;
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
          attr.name = req.semantic; // Use SEMANTIC as matching name, not GLSL NAME

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

        // First: publish THIS CSF's own writable storage images so they
        // ride the geometry cable downstream and ExtractTexture / flat
        // AUXILIARY rasterizer reads can resolve them by name. Mirrors
        // the m_storageBuffers → out_geo.buffers forward done above.
        for(const auto& si : m_storageImages)
        {
          if(si.access == "read_only" || !si.texture)
            continue;
          out_geo.auxiliary_textures.push_back(
              ossia::geometry::auxiliary_texture{
                  .name = si.name.toStdString(),
                  .native_handle = si.texture,
                  .sampler_handle = nullptr});
        }

        // Same forward for nested-aux storage images this binding
        // auto-allocated (at.owned == true). Lets a CSF declare its
        // writable storage image under the geometry-input AUXILIARY
        // block and have it published to downstream consumers
        // identically to the top-level csf_image_input case.
        for(const auto& at : binding.auxiliary_textures)
        {
          if(!at.owned || !at.texture)
            continue;
          bool already_present = false;
          for(const auto& existing : out_geo.auxiliary_textures)
            if(existing.name == at.name) { already_present = true; break; }
          if(already_present)
            continue;
          out_geo.auxiliary_textures.push_back(
              ossia::geometry::auxiliary_texture{
                  .name = at.name,
                  .native_handle = at.texture,
                  .sampler_handle = nullptr});
        }

        // Forward upstream auxiliary TEXTURES (skybox, irradiance_map,
        // baseColorArray*, normalArray*, shadow_map_array, …). Without
        // this, classic_pbr_full / classic_pbr_openpbr / any rasterizer
        // that samples material texture arrays via sample_slot_* finds
        // the bindings empty (or fallback-placeholder), every textureRef
        // resolves to placeholder-black, and every textured fragment
        // renders fully black. Same name-collision skip rule as the
        // buffer forward — if THIS CSF declared an aux texture of the
        // same name (RESOURCES.auxiliary_textures or similar), keep its
        // binding and skip the upstream re-add.
        for(const auto& in_atx : binding_upstream->auxiliary_textures)
        {
          bool already_present = false;
          for(const auto& existing : out_geo.auxiliary_textures)
            if(existing.name == in_atx.name) { already_present = true; break; }
          if(already_present)
            continue;
          out_geo.auxiliary_textures.push_back(in_atx);
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
        for(const auto& [port_key, geo_spec] : m_portGeometries)
        {
          if(!geo_spec.meshes || geo_spec.meshes->meshes.empty())
            continue;

          const int port_idx = port_key.first;

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
          continue;
        }

        const std::string& src_geo_name = attr_req.forward->geometry;
        const std::string& src_attr_name = attr_req.forward->attribute;

        for(const auto& [port_key, geo_spec] : m_portGeometries)
        {
          if(!geo_spec.meshes || geo_spec.meshes->meshes.empty())
            continue;

          const int port_idx = port_key.first;

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
            continue;
          }

          const auto& src_mesh = geo_spec.meshes->meshes[0];
          // Find the source attribute by name
          for(const auto& in_attr : src_mesh.attributes)
          {
            std::string_view attr_display = ossia::geometry::display_name(in_attr);
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

                binding.copyFromBuffers.push_back(gpu_buf);

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

      if(binding.uses_indirect_draw && binding.indirectBuffer)
      {
        out_geo.indirect_count = ossia::geometry::gpu_buffer{
            binding.indirectBuffer,
            binding.indirectBufferSize};
      }
      else if(binding_upstream
              && binding_upstream->indirect_count.handle)
      {
        // Forward upstream's indirect-draw buffer when this CSF doesn't
        // produce its own. ScenePreprocessor sets indirect_count to the
        // MDI indirect_draw_cmds buffer (ScenePreprocessorNode.cpp:2329);
        // an MDI rasterizer downstream reads from out_geo.indirect_count
        // for vkCmdDrawIndexedIndirect dispatch. Without this forward,
        // every passthrough CSF inserted between Preprocessor and an MDI
        // rasterizer hands the rasterizer a null indirect buffer →
        // garbage indexCount / firstIndex / baseVertex → triangles
        // render at wild positions / wrong index ranges.
        out_geo.indirect_count = binding_upstream->indirect_count;
      }

      // Forward CPU-side draw commands too. ScenePreprocessor populates
      // these (`cpu_draw_commands`, ScenePreprocessorNode.cpp:2334) for
      // the Qt < 6.12 / non-GPU-indirect fallback path. Without this
      // forward, CustomMesh::update sees an empty vector and skips the
      // assign() at line 370 — leaving `output_meshbuf.cpuDrawCommands`
      // with stale data from a previous frame OR uninitialised
      // small-vector contents, which the CPU draw fallback then issues
      // as drawIndexed(garbage, garbage, ...). Symptom: Vulkan
      // VUID-vkCmdDrawIndexed-robustBufferAccess2-08798 with huge
      // firstIndex/indexCount values that look like pointer low bits.
      if(binding_upstream && !binding_upstream->cpu_draw_commands.empty())
      {
        out_geo.cpu_draw_commands.assign(
            binding_upstream->cpu_draw_commands.begin(),
            binding_upstream->cpu_draw_commands.end());
      }

      // Stamp format_id from the descriptor's RESOURCES[geoOut] so a
      // CSF that produces a primitive-cloud-shaped output declares its
      // format identity in JSON and downstream FlattenedSceneFilter
      // mode-12 can route it. Same hash + truncation as the
      // ScenePreprocessor splat-bucket stamp.
      if(!geo_input->format_id.empty())
        out_geo.filter_tag = (uint32_t)ossia::hash_string(geo_input->format_id);

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

      if(binding.uses_indirect_draw && binding.indirectBuffer)
      {
        out_geo.indirect_count = ossia::geometry::gpu_buffer{
            binding.indirectBuffer,
            binding.indirectBufferSize};
      }
      else if(binding_upstream
              && binding_upstream->indirect_count.handle)
      {
        // Mirror the full-rebuild path: forward upstream's indirect-
        // draw buffer when this CSF doesn't produce its own. Required
        // for any passthrough CSF inserted in front of an MDI
        // rasterizer (ScenePreprocessor → CSF → classic_pbr_mdi /
        // openpbr / debug_lights). Without this, the fast path keeps
        // the previously-published indirect_count handle, which is
        // empty for compute passes that never set it themselves.
        if(out_geo.indirect_count.handle != binding_upstream->indirect_count.handle
           || out_geo.indirect_count.byte_size != binding_upstream->indirect_count.byte_size)
        {
          out_geo.indirect_count = binding_upstream->indirect_count;
          any_handle_changed = true;
        }
      }

      // Re-forward upstream's CPU draw commands every frame. The vector
      // contents are immutable in the typical scene flow but the
      // binding's outputGeometry mesh holds a copy that can drift if
      // upstream rebuilds (e.g. a scene reload). Cheap re-assign each
      // frame; ScenePreprocessor's command list is at most ~1k entries.
      if(binding_upstream && !binding_upstream->cpu_draw_commands.empty())
      {
        out_geo.cpu_draw_commands.assign(
            binding_upstream->cpu_draw_commands.begin(),
            binding_upstream->cpu_draw_commands.end());
      }

      // Re-forward upstream metadata that the rasterizer reads but the
      // CSF doesn't override: pipeline-state hints (blend, depth_write)
      // and filter metadata (filter_tag, filter_material_index).
      // Identity assignments — the upstream values either stayed the
      // same since the structural pass or shifted (scene reload), and
      // we want the latter to propagate.
      if(binding_upstream)
      {
        out_geo.blend                 = binding_upstream->blend;
        out_geo.depth_write           = binding_upstream->depth_write;
        out_geo.filter_tag            = binding_upstream->filter_tag;
        out_geo.filter_material_index = binding_upstream->filter_material_index;

        // Re-forward upstream auxiliary TEXTURES (skybox, baseColorArray,
        // shadow_map_array, …). Same forward as the structural-rebuild
        // path; needed every frame in case upstream rebakes (CubemapLoader
        // refresh, IBL bake, etc.). Skip names already declared by this
        // CSF or already pushed in this frame.
        out_geo.auxiliary_textures.clear();

        // Publish THIS CSF's own writable storage images (write_only and
        // read_write csf_image_input declarations) into the geometry
        // cable's auxiliary_textures so downstream consumers (ExtractTexture
        // node, rasterizers reading them as flat AUXILIARY) can resolve
        // them by name. Without this push, the texture exists in this
        // CSF's m_storageImages but is invisible to the world — the
        // mirror of how m_storageBuffers is forwarded into out_geo.buffers
        // a few lines above.
        for(const auto& si : m_storageImages)
        {
          if(si.access == "read_only" || !si.texture)
            continue;
          out_geo.auxiliary_textures.push_back(
              ossia::geometry::auxiliary_texture{
                  .name = si.name.toStdString(),
                  .native_handle = si.texture,
                  .sampler_handle = nullptr});
        }

        // Same forward for write_only / read_write storage images
        // declared as nested aux on the geometry input (auto-allocated
        // in the binding setup with at.owned = true). Required for
        // voxelize_scene_aabb.csf's `voxel_grid` to ship downstream
        // when declared as a nested aux on the scene geometry input
        // rather than as a top-level csf_image_input.
        for(const auto& at : binding.auxiliary_textures)
        {
          if(!at.owned || !at.texture)
            continue;
          bool already_present = false;
          for(const auto& existing : out_geo.auxiliary_textures)
            if(existing.name == at.name) { already_present = true; break; }
          if(already_present)
            continue;
          out_geo.auxiliary_textures.push_back(
              ossia::geometry::auxiliary_texture{
                  .name = at.name,
                  .native_handle = at.texture,
                  .sampler_handle = nullptr});
        }

        // Then forward upstream auxiliary textures, skipping any name
        // this CSF already published above so producer-side overrides
        // win over upstream defaults (consistent with the buffer-forward
        // shadowing rule).
        for(const auto& in_atx : binding_upstream->auxiliary_textures)
        {
          bool already_present = false;
          for(const auto& existing : out_geo.auxiliary_textures)
            if(existing.name == in_atx.name) { already_present = true; break; }
          if(already_present)
            continue;
          out_geo.auxiliary_textures.push_back(in_atx);
        }
      }

      // Only bump dirty_index if any handle actually changed,
      // so downstream acquireMesh picks up the new buffers.
      if(any_handle_changed)
      {
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
        rendered->second->process(port_idx, binding.outputGeometry, out_edge->source);
      }
    }

    geo_binding_idx++;
    geo_output_idx++;
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

    // Compile one pipeline per unique LOCAL_SIZE, reuse when passes share the same size.
    m_perPassPipelines.clear();
    std::map<std::array<int,3>, QRhiComputePipeline*> pipelineCache;

    for(std::size_t passIdx = 0; passIdx < n.m_descriptor.csf_passes.size(); passIdx++)
    {
      const auto& passDesc = n.m_descriptor.csf_passes[passIdx];
      const auto key = passDesc.local_size;

      auto it = pipelineCache.find(key);
      if(it != pipelineCache.end())
      {
        // Reuse existing pipeline
        m_perPassPipelines.push_back(it->second);
      }
      else
      {
        // Compile new pipeline for this local_size
        QString src = m_computeShaderSource;
        src.replace("ISF_LOCAL_SIZE_X", QString::number(key[0]));
        src.replace("ISF_LOCAL_SIZE_Y", QString::number(key[1]));
        src.replace("ISF_LOCAL_SIZE_Z", QString::number(key[2]));

        QShader compiled = score::gfx::makeCompute(renderer.state, src);

        auto* pipeline = rhi.newComputePipeline();
        pipeline->setShaderStage(QRhiShaderStage(QRhiShaderStage::Compute, compiled));

        pipelineCache[key] = pipeline;
        m_perPassPipelines.push_back(pipeline);
      }
    }

    // Store unique pipelines for cleanup
    m_ownedPipelines.clear();
    for(auto& [k, v] : pipelineCache)
      m_ownedPipelines.push_back(v);

    // For backward compat
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

void RenderedCSFNode::buildComputeSrbBindings(
    RenderList& renderer, QRhiResourceUpdateBatch& res,
    QList<QRhiShaderResourceBinding>& bindings)
{
  QRhi& rhi = *renderer.state.rhi;

  // Pre-pass: collect physical buffers used with conflicting access modes
  // (read on one binding, write on another) so we can promote them to
  // bufferLoadStore. The Qt RHI / Vulkan validation layer rejects bindings
  // that reference the same buffer with different access flags within a pass.
  std::unordered_set<QRhiBuffer*> aliased_buffers;
  {
    std::unordered_map<QRhiBuffer*, int> access_flags; // 1=read, 2=write, 3=both
    int gb_idx = 0;
    for(const auto& inp : n.m_descriptor.inputs)
    {
      auto* g = ossia::get_if<isf::geometry_input>(&inp.data);
      if(!g)
        continue;
      if(gb_idx >= (int)m_geometryBindings.size())
        break;
      const auto& gb = m_geometryBindings[gb_idx++];

      for(int ai = 0; ai < (int)g->attributes.size() && ai < (int)gb.attribute_ssbos.size(); ai++)
      {
        const auto& req = g->attributes[ai];
        const auto& ssbo = gb.attribute_ssbos[ai];
        if(req.access == "none" || !ssbo.buffer)
          continue;
        int f = (req.access == "read_only") ? 1 : (req.access == "write_only") ? 2 : 3;
        access_flags[ssbo.buffer] |= f;
        if(req.access == "read_write" && ssbo.read_buffer && ssbo.read_buffer != ssbo.buffer)
          access_flags[ssbo.read_buffer] |= 1;
      }
      for(const auto& aux : gb.auxiliary_ssbos)
      {
        if(!aux.buffer)
          continue;
        int f = (aux.access == "read_only") ? 1 : (aux.access == "write_only") ? 2 : 3;
        access_flags[aux.buffer] |= f;
        if(aux.read_buffer && aux.read_buffer != aux.buffer)
          access_flags[aux.read_buffer] |= 1;
      }
    }
    for(const auto& [buf, flags] : access_flags)
      if(flags == 3)
        aliased_buffers.insert(buf);
  }

  // Binding 0: Renderer UBO (part of ProcessUBO in defaultUniforms)
  bindings.append(QRhiShaderResourceBinding::uniformBuffer(
      0, QRhiShaderResourceBinding::ComputeStage, &renderer.outputUBO()));

  // Binding 1: Process UBO (time, passIndex, etc.)
  // Per-pass: actual pointer is patched by each caller after this returns.
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
        // Missing storage buffer: warn (used to be silent on the recreate
        // path / qDebug on the init path — unify to qWarning) and bump
        // bindingIndex so the rest of the layout stays in sync with the
        // shader's expected slots.
        if(it == m_storageBuffers.end())
          qWarning() << "CSF: storage buffer not found for input"
                     << QString::fromStdString(input.name);
        else
          qWarning() << "CSF: cannot bind null buffer for input"
                     << QString::fromStdString(input.name);
        bindingIndex++;
      }
    }
    // Regular textures (sampled)
    else if(ossia::get_if<isf::texture_input>(&input.data))
    {
      // Regular sampled textures from m_inputSamplers
      if(input_image_index < m_inputSamplers.size())
      {
        auto [sampler, tex, fb_] = m_inputSamplers[input_image_index];
        if(sampler && tex)
        {
          bindings.append(
              QRhiShaderResourceBinding::sampledTexture(
                  bindingIndex, QRhiShaderResourceBinding::ComputeStage, tex, sampler));
        }
        else
        {
          qWarning() << "CSF: sampler/texture missing for texture_input"
                     << QString::fromStdString(input.name);
        }
      }
      else
      {
        qWarning() << "CSF: input_samplers under-allocated for texture_input"
                   << QString::fromStdString(input.name);
      }
      // Always bump bindingIndex to keep the shader-layout slot count stable.
      bindingIndex++;
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
            auto [sampler, tex, fb_] = m_inputSamplers[input_image_index];
            if(tex)
            {
              bindings.append(
                  QRhiShaderResourceBinding::imageLoad(
                      bindingIndex, QRhiShaderResourceBinding::ComputeStage, tex, 0));
            }
            else
            {
              qWarning() << "CSF: missing read_only image texture for"
                         << QString::fromStdString(input.name);
            }
          }
          else
          {
            qWarning() << "CSF: input_samplers under-allocated for csf_image_input"
                       << QString::fromStdString(input.name);
          }
          bindingIndex++;
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

          // Lazy-allocate the storage-image texture (and its persistent
          // _prev twin) on first emission. After init this branch is a
          // no-op (it->texture is already set), so the recreate path
          // re-emits against the existing handle.
          auto make_tex = [&](const char* suffix) -> QRhiTexture* {
            QRhiTexture* t{};
            if(image->isCube())
            {
              const int edge
                  = std::max(imageSize.width(), imageSize.height());
              QRhiTexture::Flags flags
                  = QRhiTexture::CubeMap | QRhiTexture::UsedWithLoadStore;
              t = rhi.newTexture(format, QSize(edge, edge), 1, flags);
            }
            else if(image->is3D())
            {
              int depth = !image->depth_expression.empty()
                  ? resolveDispatchExpression(image->depth_expression)
                  : imageSize.height();
              QRhiTexture::Flags flags
                  = QRhiTexture::ThreeDimensional | QRhiTexture::UsedWithLoadStore;
              t = rhi.newTexture(
                  format, imageSize.width(), imageSize.height(), depth, 1, flags);
            }
            else if(image->is_array)
            {
              int layers = !image->layers_expression.empty()
                  ? resolveDispatchExpression(image->layers_expression)
                  : 1;
              if(layers < 1) layers = 1;
              QRhiTexture::Flags flags = QRhiTexture::UsedWithLoadStore;
              t = rhi.newTextureArray(format, layers, imageSize, 1, flags);
            }
            else
            {
              QRhiTexture::Flags flags
                  = QRhiTexture::RenderTarget | QRhiTexture::UsedWithLoadStore
                    | QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips;
              t = rhi.newTexture(format, imageSize, 1, flags);
            }
            t->setName(
                ("RenderedCSFNode::storageImage::" + input.name + suffix).c_str());
            if(!t->create())
            {
              delete t;
              return nullptr;
            }
            return t;
          };

          if(!it->texture)
          {
            it->texture = make_tex("");
            if(it->texture && !m_outputTexture)
            {
              m_outputTexture = it->texture;
              m_outputFormat = format;
            }
          }
          if(it->persistent && !it->read_texture)
            it->read_texture = make_tex("_prev");

          it->binding = bindingIndex;
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
            if(!it->texture)
              qWarning() << "CSF: missing storage-image texture for"
                         << QString::fromStdString(input.name);
            bindingIndex++; // keep indices synchronized with shader layout
          }

          // Persistent pair: `<name>_prev` readonly at the adjacent slot.
          // First frame aliases back to `texture` (no prior frame to read).
          if(it->persistent)
          {
            QRhiTexture* prev_tex
                = it->pending_initial_copy ? it->texture : it->read_texture;
            if(!prev_tex)
              prev_tex = it->texture;
            it->prev_binding = bindingIndex;
            if(prev_tex)
            {
              bindings.append(
                  QRhiShaderResourceBinding::imageLoad(
                      bindingIndex++, QRhiShaderResourceBinding::ComputeStage,
                      prev_tex, 0));
            }
            else
            {
              qWarning() << "CSF: missing persistent _prev texture for"
                         << QString::fromStdString(input.name);
              bindingIndex++;
            }
          }
          output_port_index++;
          output_image_index++;
        }
      }
      else
      {
        qWarning() << "CSF: storage image not found for"
                   << QString::fromStdString(input.name);
        bindingIndex++;
        if(image->persistent)
          bindingIndex++;
      }
    }
    // Geometry inputs: bind per-attribute SSBOs
    else if(auto* geo_input = ossia::get_if<isf::geometry_input>(&input.data))
    {
      if(geo_binding_index < (int)m_geometryBindings.size())
      {
        auto& binding = m_geometryBindings[geo_binding_index];

        // Helper: emit a binding for buf with the given access mode, promoting
        // to bufferLoadStore when the buffer is aliased across multiple bindings
        // with conflicting accesses (avoids Vulkan validation warnings).
        auto appendBufBinding = [&](QRhiBuffer* buf, const std::string& access)
        {
          const bool aliased = aliased_buffers.count(buf) > 0;
          if(access == "read_write" || aliased)
          {
            bindings.append(QRhiShaderResourceBinding::bufferLoadStore(
                bindingIndex++, QRhiShaderResourceBinding::ComputeStage, buf));
          }
          else if(access == "read_only")
          {
            bindings.append(QRhiShaderResourceBinding::bufferLoad(
                bindingIndex++, QRhiShaderResourceBinding::ComputeStage, buf));
          }
          else // write_only
          {
            bindings.append(QRhiShaderResourceBinding::bufferStore(
                bindingIndex++, QRhiShaderResourceBinding::ComputeStage, buf));
          }
        };

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
            // Create a minimal fallback buffer so we don't skip a binding
            // index. Same fallback shape for both init and re-emit paths
            // (the buffer name encodes the call site for debug clarity).
            const int64_t elem_stride = std430ArrayStride(req.type, n.m_descriptor);
            ssbo.buffer = rhi.newBuffer(
                QRhiBuffer::Static,
                QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer, elem_stride);
            ssbo.buffer->setName(QByteArray("CSF_GeomFB_") + req.name.c_str());
            ssbo.buffer->create();
            ssbo.size = elem_stride;
            ssbo.owned = true;
          }

          if(req.access == "read_only" || req.access == "write_only")
          {
            appendBufBinding(ssbo.buffer, req.access);
          }
          else // read_write -> 2 bindings: _in (readonly) + _out (read-write)
          {
            // On the first feedback frame (pending_initial_copy), use the same
            // buffer for both _in and _out so the shader can init + simulate
            // in the same frame.  After the frame we copy buffer->read_buffer.
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
              appendBufBinding(read_buf, "read_only");
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
            // Create a minimal fallback buffer so we don't skip a binding
            // index. Usage flag must match the aux kind — binding a
            // StorageBuffer-only buffer as a UBO (or vice versa) is
            // rejected by the Vulkan validation layer.
            const auto fallback_usage = aux.is_uniform
                ? QRhiBuffer::UniformBuffer
                : QRhiBuffer::StorageBuffer;
            const quint32 fallback_size = aux.is_uniform ? 256u : 16u;
            aux.buffer = rhi.newBuffer(
                QRhiBuffer::Static, fallback_usage, fallback_size);
            aux.buffer->setName(QByteArray("CSF_AuxFB_") + aux.name.c_str());
            aux.buffer->create();
            aux.size = fallback_size;
            aux.owned = true;
          }

          if(aux.is_uniform)
          {
            // std140 UBO kind: bind as uniform, not load/store. Access
            // field is ignored (UBOs are read-only in GLSL).
            bindings.append(
                QRhiShaderResourceBinding::uniformBuffer(
                    bindingIndex++, QRhiShaderResourceBinding::ComputeStage,
                    aux.buffer));
          }
          else
          {
            appendBufBinding(aux.buffer, aux.access);
          }
        }

        // Auxiliary textures for this geometry input — placed right
        // after aux SSBOs, matching the GLSL emission order in
        // parse_csf. Sampled entries → sampledTexture binding; storage
        // entries → imageLoad / imageStore / imageLoadStore per access.
        for(auto& at : binding.auxiliary_textures)
        {
          if(!at.texture)
            at.texture = at.placeholder;

          QRhiShaderResourceBinding b;
          if(at.is_storage)
          {
            if(at.access == "read_only")
              b = QRhiShaderResourceBinding::imageLoad(
                  bindingIndex, QRhiShaderResourceBinding::ComputeStage,
                  at.texture, 0);
            else if(at.access == "write_only")
              b = QRhiShaderResourceBinding::imageStore(
                  bindingIndex, QRhiShaderResourceBinding::ComputeStage,
                  at.texture, 0);
            else
              b = QRhiShaderResourceBinding::imageLoadStore(
                  bindingIndex, QRhiShaderResourceBinding::ComputeStage,
                  at.texture, 0);
          }
          else
          {
            b = QRhiShaderResourceBinding::sampledTexture(
                bindingIndex, QRhiShaderResourceBinding::ComputeStage,
                at.texture, at.sampler);
          }
          bindings.append(b);
          at.binding = bindingIndex;
          bindingIndex++;
        }

        if(binding.uses_indirect_draw && binding.indirectBuffer)
        {
          bindings.append(QRhiShaderResourceBinding::bufferLoadStore(
              bindingIndex++, QRhiShaderResourceBinding::ComputeStage,
              binding.indirectBuffer));
        }

        geo_binding_index++;
      }
      // Inlet port for upstream geometry. Two cases create one:
      //   - Empty ATTRIBUTES => pure pass-through: ISFNode unconditionally
      //     pushes an input port (the visitor at ISFNode.cpp's
      //     `if(in.attributes.empty())` branch).
      //   - Non-empty ATTRIBUTES with at least one read_only / read_write
      //     attribute => an upstream-feeding inlet.
      // Either way the geometry input owns ONE entry in node.input,
      // which subsequent storage_input / texture_input / etc. address by
      // position. Without this increment the very next read_only
      // storage_input picks up node.input[0] (the geometry port) by
      // mistake — its edges point to upstream geometry, bufferForInput
      // returns empty, and the storage_input falls back to its own
      // zero-initialised dummy buffer. Symptom: storage data from the
      // upstream cable never reaches the compute shader.
      bool geo_creates_inlet = geo_input->attributes.empty();
      if(!geo_creates_inlet)
      {
        for(const auto& attr : geo_input->attributes)
        {
          if(attr.access == "read_only" || attr.access == "read_write")
          {
            geo_creates_inlet = true;
            break;
          }
        }
      }
      if(geo_creates_inlet)
        input_port_index++;
      // Skip $USER ports for this geometry input
      if(geo_input->vertex_count.find("$USER") != std::string::npos) input_port_index++;
      if(geo_input->instance_count.find("$USER") != std::string::npos) input_port_index++;
      for(const auto& aux : geo_input->auxiliary)
        if(aux.size.find("$USER") != std::string::npos) input_port_index++;
      if(geo_input->indirect && geo_input->indirect->count.find("$USER") != std::string::npos) input_port_index++;
    }
    else
    {
      input_port_index++;
    }
  }
}

void RenderedCSFNode::initComputeSRBAndPasses(
    RenderList& renderer, QRhiResourceUpdateBatch& res)
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

  // Eagerly populate geometry bindings so we can detect buffer aliasing across
  // attribute/auxiliary SSBOs (caused by feedback edges sharing the same
  // physical buffer with conflicting access modes) BEFORE we emit any binding.
  updateGeometryBindings(renderer, res);

  // Single source of truth for the bindings list (also used by
  // recreateShaderResourceBindings — see buildComputeSrbBindings).
  QList<QRhiShaderResourceBinding> bindings;
  buildComputeSrbBindings(renderer, res, bindings);

  // Set the SRB on the pipeline and create it
  {
    // Create one ComputePass entry for each CSF pass, each with their own pipeline, ProcessUBO and SRB
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
      QRhiShaderResourceBindings* passSRB = rhi.newShaderResourceBindings();
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
          nullptr, ComputePass{passPipeline, passSRB, passProcessUBO});
    }
  }
}

void RenderedCSFNode::initState(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  QRhi& rhi = *renderer.state.rhi;

  // Reset the "first frame" gate so that generateMips() in update() waits
  // for the upstream pass to actually write the input textures before being
  // called -- see the matching comment in update().
  m_inputsHaveBeenWritten = false;

  // Check for compute support
  if(!rhi.isFeatureSupported(QRhi::Compute))
  {
    qWarning() << "Compute shaders not supported on this backend";
    return;
  }

  // ProcessUBO will be created per-pass in initComputeSRBAndPasses

  // Initialize GPU buffer scatter for format conversion
  m_gpuScatterAvailable = m_gpuScatter.init(renderer.state);

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
    else if(n.m_material_data)
    {
      res.updateDynamicBuffer(m_materialUBO, 0, m_materialSize, n.m_material_data.get());
    }
  }

  // Initialize input samplers
  SCORE_ASSERT(m_computePasses.empty());
  SCORE_ASSERT(m_inputSamplers.empty());

  // Create samplers for input textures
  m_inputSamplers = initInputSamplers(this->n, renderer, n.input, &n.descriptor());

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
      StorageImage si;
      si.name = QString::fromStdString(input.name);
      si.access = QString::fromStdString(image->access);
      si.format = format;
      si.is3D = image->is3D();
      si.isCube = image->isCube();
      si.persistent = image->persistent;
      si.pending_initial_copy = image->persistent;
      // generateMips is only meaningful on plain 2D images — QRhi doesn't
      // define a mip chain for 3D, cubemaps would need per-face generation
      // that QRhi::generateMips doesn't promise across backends, and 2D
      // arrays similarly have per-layer semantics that aren't guaranteed.
      // Silently disable the flag outside of plain 2D so downstream samplers
      // don't hit a no-op they might have expected to work.
      si.generate_mips = image->generate_mips && !image->is3D()
                         && !image->isCube() && !image->is_array;
      m_storageImages.push_back(si);

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
      binding.input_name = input.name;
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
          const int64_t elem_stride = std430ArrayStride(geo->attributes[attr_idx].type, n.m_descriptor);
          const int64_t needed = elem_stride * count;
          auto* buf = rhi.newBuffer(
              QRhiBuffer::Static,
              QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer, needed);
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
        ssbo.is_uniform = aux.is_uniform;
        ssbo.layout = aux.layout;
        ssbo.size_expr = aux.size;

        // Create the buffer immediately so it's available for the first dispatch.
        // Usage flag matches the aux kind — UBO path uses UniformBuffer,
        // SSBO path uses StorageBuffer. Using the wrong usage flag is a
        // Vulkan validation error at bind time.
        int arrayCount = 0;
        if(!aux.size.empty())
          arrayCount = resolveCountExpression(aux.size, *geo, aux.name);

        const int64_t requiredSize = score::gfx::calculateStorageBufferSize(
            aux.layout, arrayCount, this->n.descriptor());
        if(requiredSize > 0)
        {
          const auto usage = aux.is_uniform ? QRhiBuffer::UniformBuffer
                                            : QRhiBuffer::StorageBuffer;
          auto* buf = rhi.newBuffer(QRhiBuffer::Static, usage, requiredSize);
          buf->setName(QByteArray("CSF_GeoAux_") + aux.name.c_str());
          buf->create();
          QByteArray zero(requiredSize, 0);
          res.uploadStaticBuffer(buf, 0, requiredSize, zero.constData());
          ssbo.buffer = buf;
          ssbo.size = requiredSize;
          ssbo.owned = true;
        }

        binding.auxiliary_ssbos.push_back(std::move(ssbo));

        // UBOs are inherently read-only from GLSL, so they never flag
        // has_output. For SSBOs, any non-read_only access opts in.
        if(!aux.is_uniform && aux.access != "read_only")
          binding.has_output = true;
      }

      // Auxiliary textures: one entry per geometry_input AUXILIARY
      // texture declaration. Sampler allocated now (or skipped for
      // storage-image entries); placeholder texture picked from the
      // RenderList empties so the SRB is always valid even before an
      // upstream resolution happens. Per-frame resolution against
      // ossia::geometry::auxiliary_textures happens in
      // updateGeometryBindings.
      //
      // For write_only / read_write storage-image entries this binding
      // ALSO allocates the actual texture itself (analog of the
      // m_storageImages allocation that top-level csf_image_input
      // entries get). Without this auto-alloc the binding stays glued
      // to the RGBA8-typed sample-only emptyTexture3D placeholder and
      // any imageStore / imageAtomicOr against an integer-formatted
      // shader (uimage3D r32ui) trips Vulkan validation 00339 (no
      // STORAGE_BIT) + 07753 (UINT vs UNORM) + 02691 (no atomic
      // format feature).
      for(const auto& atx : geo->auxiliary_textures)
      {
        RenderedCSFNode::GeometryBinding::AuxiliaryTexture at;
        at.name = atx.name;
        at.is_storage = atx.is_storage;
        at.access = atx.access;

        if(!atx.is_storage)
        {
          at.sampler = score::gfx::makeSampler(rhi, atx.sampler);
          at.sampler->setName(
              QByteArray("CSF_AuxTex_sampler::") + atx.name.c_str());
        }

        if(atx.is_cubemap)
          at.placeholder = &renderer.emptyTextureCube();
        else if(atx.dimensions == 3)
          at.placeholder = &renderer.emptyTexture3D();
        else if(atx.is_array)
          at.placeholder = &renderer.emptyTextureArray();
        else
          at.placeholder = &renderer.emptyTexture();
        at.texture = at.placeholder;

        // Auto-allocate writable storage image. Resolves the size
        // expressions (WIDTH/HEIGHT/DEPTH/LAYERS) the same way
        // computeTextureSize does for top-level csf_image_input entries.
        if(atx.is_storage && atx.access != "read_only")
        {
          QRhiTexture::Format format = getTextureFormat(
              QString::fromStdString(atx.format));

          int w = !atx.width_expression.empty()
              ? std::max(1, resolveDispatchExpression(atx.width_expression))
              : renderer.state.renderSize.width();
          int h = !atx.height_expression.empty()
              ? std::max(1, resolveDispatchExpression(atx.height_expression))
              : renderer.state.renderSize.height();

          QRhiTexture* alloc = nullptr;
          if(atx.is_cubemap)
          {
            const int edge = std::max(w, h);
            alloc = rhi.newTexture(
                format, QSize(edge, edge), 1,
                QRhiTexture::CubeMap | QRhiTexture::UsedWithLoadStore);
          }
          else if(atx.dimensions == 3)
          {
            int d = !atx.depth_expression.empty()
                ? std::max(1, resolveDispatchExpression(atx.depth_expression))
                : h;  // square cube fallback
            alloc = rhi.newTexture(
                format, w, h, d, 1,
                QRhiTexture::ThreeDimensional | QRhiTexture::UsedWithLoadStore);
          }
          else if(atx.is_array)
          {
            int layers = !atx.layers_expression.empty()
                ? std::max(1, resolveDispatchExpression(atx.layers_expression))
                : 1;
            alloc = rhi.newTextureArray(
                format, layers, QSize(w, h), 1,
                QRhiTexture::UsedWithLoadStore);
          }
          else
          {
            alloc = rhi.newTexture(
                format, QSize(w, h), 1,
                QRhiTexture::UsedWithLoadStore);
          }

          if(alloc)
          {
            alloc->setName(
                ("CSF::auxStorageImage::" + atx.name).c_str());
            if(alloc->create())
            {
              at.texture = alloc;
              at.owned = true;
            }
            else
            {
              delete alloc;
            }
          }
        }

        binding.auxiliary_textures.push_back(std::move(at));
      }

      if(geo->indirect)
      {
        binding.uses_indirect_draw = true;
        binding.indirectCountExpr = geo->indirect->count;

        int count = resolveCountExpression(geo->indirect->count, *geo, "__indirect_count__");
        if(count <= 0) count = 1;
        binding.indirectCountResult = count;

        const int64_t indirectSize = (int64_t)count * 5 * sizeof(uint32_t);

        auto usageFlags = QRhiBuffer::StorageBuffer;
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
        usageFlags |= QRhiBuffer::IndirectBuffer;
#endif

        auto* buf = rhi.newBuffer(QRhiBuffer::Static, usageFlags, indirectSize);
        buf->setName(QByteArray("CSF_Indirect_") + input.name.c_str());
        buf->create();

        QByteArray zero(indirectSize, 0);
        res.uploadStaticBuffer(buf, 0, indirectSize, zero.constData());

        binding.indirectBuffer = buf;
        binding.indirectBufferSize = indirectSize;
      }

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
      if(geo->indirect && geo->indirect->count.find("$USER") != std::string::npos)
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

  // Create the compute passes (edge-independent: SRB, pipelines, processUBOs)
  initComputeSRBAndPasses(renderer, res);

  m_initialized = true;
}

void RenderedCSFNode::addOutputPass(
    RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res)
{
  if(!m_initialized)
    return;

  const auto& rt = renderer.renderTargetForOutput(edge);
  if(rt.renderTarget)
  {
    createGraphicsPass(rt, renderer, edge, res);
  }
}

void RenderedCSFNode::removeOutputPass(RenderList& renderer, Edge& edge)
{
  auto it = ossia::find_if(
      m_graphicsPasses, [&](const auto& p) { return p.first == &edge; });
  if(it != m_graphicsPasses.end())
  {
    it->second.pipeline.release();
    delete it->second.outputSampler;
    m_graphicsPasses.erase(it);
  }
}

bool RenderedCSFNode::hasOutputPassForEdge(Edge& edge) const
{
  return ossia::find_if(
             m_graphicsPasses, [&](const auto& p) { return p.first == &edge; })
         != m_graphicsPasses.end();
}

void RenderedCSFNode::releaseState(RenderList& r)
{
  if(!m_initialized)
    return;

  // Clean up remaining graphics passes
  for(auto& [edge, pass] : m_graphicsPasses)
  {
    pass.pipeline.release();
    delete pass.outputSampler;
  }
  m_graphicsPasses.clear();

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

  // Clean up pipelines (m_ownedPipelines has unique entries, m_perPassPipelines may have duplicates)
  for(auto* pip : m_ownedPipelines)
    delete pip;
  m_ownedPipelines.clear();
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
    for(auto& at : binding.auxiliary_textures)
    {
      if(at.sampler)
        at.sampler->deleteLater();
      at.sampler = nullptr;
      // For owned textures (auto-allocated writable storage images),
      // we created the QRhiTexture and must release it here. Sampled
      // entries point to either a RenderList-owned placeholder or an
      // upstream-geometry-owned handle — those we don't free.
      if(at.owned && at.texture)
        at.texture->deleteLater();
      at.texture = nullptr;
      at.owned = false;
    }
    binding.auxiliary_textures.clear();
    for(auto* buf : binding.copyFromBuffers)
      r.releaseBuffer(buf);
    binding.copyFromBuffers.clear();
    if(binding.indirectBuffer)
    {
      r.releaseBuffer(binding.indirectBuffer);
      binding.indirectBuffer = nullptr;
    }
  }
  m_geometryBindings.clear();

  // Clean up storage images (including persistent ping-pong pair)
  for(auto& storageImage : m_storageImages)
  {
    if(storageImage.texture)
      storageImage.texture->deleteLater();
    if(storageImage.read_texture)
      storageImage.read_texture->deleteLater();
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
    // texture is deleted elsewhere
  }
  m_inputSamplers.clear();

  m_initialized = false;
}

void RenderedCSFNode::addInputEdge(
    RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res)
{
  if(edge.sink->type == Types::Image)
  {
    // Find upstream texture
    if(auto it = edge.source->node->renderedNodes.find(&renderer);
       it != edge.source->node->renderedNodes.end())
    {
      if(auto* tex = it->second->textureForOutput(*edge.source))
      {
        auto rt = renderer.renderTargetForInputPort(*edge.sink);
        updateInputTexture(*edge.sink, tex, rt.depthTexture);
      }
    }
  }
  // Geometry input edges will be picked up by updateGeometryBindings in update()
}

void RenderedCSFNode::removeInputEdge(RenderList& renderer, Edge& edge)
{
  if(edge.sink->type == Types::Image)
  {
    // See SimpleRenderedISFNode::removeInputEdge — same dangling-depth-
    // sampler issue applies here when DEPTH: true inputs get disconnected.
    const bool hasDepthCompanion
        = (edge.sink->flags & Flag::SamplableDepth) == Flag::SamplableDepth;
    QRhiTexture* depthFallback
        = hasDepthCompanion ? &renderer.emptyTexture() : nullptr;
    updateInputTexture(*edge.sink, &renderer.emptyTexture(), depthFallback);
  }
  // Geometry input edges will be picked up by updateGeometryBindings in update()
}

void RenderedCSFNode::init(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  initState(renderer, res);

  // Create graphics passes for each output edge
  for(auto* output_port : n.output)
  {
    for(Edge* edge : output_port->edges)
    {
      addOutputPass(renderer, *edge, res);
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
    // CSF uploads the material UBO every frame (no materialChanged gate),
    // so resetting event ports here is enough — the zero value will
    // propagate to the GPU on the next frame's update().
    (void)n.resetEventPortsAfterFrame();
  }

  for(auto& [sampler, texture, fb_] : this->m_inputSamplers)
  {
    // Skip generateMips on textures that have not yet been written to.
    // Their Vulkan layout is still VK_IMAGE_LAYOUT_PREINITIALIZED, and Qt RHI's
    // GenMips path (qrhivulkan.cpp ~4685) transitions FROM TRANSFER_*_OPTIMAL
    // back to the texture's stored layout — which would be PREINITIALIZED here,
    // an invalid newLayout per VUID-VkImageMemoryBarrier-newLayout-01198.
    // Also skip non-mipmapped textures: nothing to generate.
    if(!texture)
      continue;
    if(!(texture->flags() & QRhiTexture::MipMapped))
      continue;
    if(!m_inputsHaveBeenWritten)
      continue;
    res.generateMips(texture);
  }
  // After this update completes, the upstream nodes will run their render
  // passes for the current frame and the input textures will be transitioned
  // out of PREINITIALIZED — so the *next* update() can safely generate mips.
  m_inputsHaveBeenWritten = true;
  
  // Update output texture size if it has changed
  // TODO: Check if texture size inputs have changed and recreate texture if needed
}

// Hash the bindings list to detect frame-to-frame drift. Two binding
// lists hash to the same value iff every entry's descriptor identity
// matches — recreateShaderResourceBindings then skips the
// destroy+setBindings+create dance when the per-pass binding list
// hasn't actually changed since the previous frame (steady state for
// a static scene; every frame would otherwise thrash the SRB pool slot).
// Use Qt's own qHash(QRhiShaderResourceBinding) so the equivalence
// matches QRhi's internal canonical representation — no need to pack
// the private Data union by hand and risk drift on a Qt minor update.
// Per-binding hashes are seeded by the binding's index so two
// otherwise-equal bindings at different slots hash differently;
// combined via ossia::hash_bytes over the per-binding hash vector.
namespace
{
uint64_t hashBindings(const QList<QRhiShaderResourceBinding>& bindings) noexcept
{
  std::vector<size_t> per;
  per.reserve(bindings.size());
  size_t i = 0;
  for(const auto& b : bindings)
    per.push_back(qHash(b, /*seed=*/i++));
  return ossia::hash_bytes(per.data(), per.size() * sizeof(size_t));
}
} // namespace

void RenderedCSFNode::recreateShaderResourceBindings(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  QRhi& rhi = *renderer.state.rhi;

  // Single source of truth for the bindings list (also used by
  // initComputeSRBAndPasses — see buildComputeSrbBindings). Geometry bindings
  // are assumed up-to-date here: the caller (update()) runs
  // updateGeometryBindings before calling this function.
  QList<QRhiShaderResourceBinding> bindings;
  buildComputeSrbBindings(renderer, res, bindings);

  // Recreate SRBs for each compute pass — but only when the per-pass
  // binding list actually changed. Hash the bindings (post per-pass
  // ProcessUBO patch) and compare to the cached hash from the previous
  // frame: identical → skip the destroy+setBindings+create cycle, which
  // would otherwise thrash the QRhi SRB pool slot every frame on a
  // static scene.
  for(auto& [edge, pass] : m_computePasses)
  {
    // Set the ProcessUBO binding for this pass — must happen BEFORE
    // hashing so a change in pass.processUBO triggers a rebuild.
    if(pass.processUBO)
    {
      bindings[1] = QRhiShaderResourceBinding::uniformBuffer(
          1, QRhiShaderResourceBinding::ComputeStage, pass.processUBO);
    }

    const uint64_t newHash = hashBindings(bindings);
    if(pass.srb && pass.srbBindingsHash == newHash && newHash != 0)
      continue; // bindings unchanged from last frame

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

    pass.srb->setBindings(bindings.cbegin(), bindings.cend());
    if(!pass.srb->create())
    {
      qWarning() << "Failed to recreate SRB for compute pass";
      delete pass.srb;
      pass.srb = nullptr;
      pass.srbBindingsHash = 0;
      continue;
    }
    pass.srbBindingsHash = newHash;
  }

  // Update the pipeline with one of the SRBs (they're all compatible)
  if(!m_computePasses.empty() && m_computePasses[0].second.srb)
  {
    m_computePipeline->setShaderResourceBindings(m_computePasses[0].second.srb);
  }
}

void RenderedCSFNode::release(RenderList& r)
{
  releaseState(r);
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
  // Plan 09 S6: debug marker for capture-tool readability.
  commands.debugMarkBegin(QByteArrayLiteral("CSF"));
  struct MarkEnd
  {
    QRhiCommandBuffer* c;
    ~MarkEnd() { c->debugMarkEnd(); }
  } _me{&commands};

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

    // Use pass-specific local sizes
    int localX = passDesc.local_size[0];
    int localY = passDesc.local_size[1];
    int localZ = passDesc.local_size[2];

    int dispatchX{}, dispatchY{}, dispatchZ{};

    // Resolve per-axis stride expressions
    const int strideX = resolveDispatchExpression(passDesc.stride[0]);
    const int strideY = resolveDispatchExpression(passDesc.stride[1]);
    const int strideZ = resolveDispatchExpression(passDesc.stride[2]);

    // Resolve the texture that drives 2D_IMAGE / 3D_IMAGE dispatch sizing.
    // Priority: pass's explicit TARGET (matches by name against both storage
    // images and input samplers) → m_outputTexture fallback.
    auto resolveDispatchTexture
        = [&]() -> QRhiTexture* {
      const auto& target = passDesc.target_resource;
      if(!target.empty())
      {
        const QString qtarget = QString::fromStdString(target);
        for(const auto& si : m_storageImages)
          if(si.name == qtarget && si.texture)
            return si.texture;

        // INPUTS entry: walk descriptor.inputs looking for a named image/texture
        // input and map it to the corresponding sampled texture.
        const auto& desc = n.descriptor();
        int input_image_index = 0;
        for(const auto& inp : desc.inputs)
        {
          const bool is_texture = ossia::get_if<isf::texture_input>(&inp.data);
          const auto* ci = ossia::get_if<isf::csf_image_input>(&inp.data);
          const bool is_img_sampled = ci && ci->access == "read_only";
          if(is_texture || is_img_sampled)
          {
            if(inp.name == target
               && input_image_index < (int)m_inputSamplers.size()
               && m_inputSamplers[input_image_index].texture)
              return m_inputSamplers[input_image_index].texture;
            input_image_index++;
          }
          else if(ossia::get_if<isf::image_input>(&inp.data))
          {
            // ISF image_input is also bound as a sampler
            input_image_index++;
          }
        }
      }
      return m_outputTexture;
    };

    // Calculate dispatch size based on execution model
    if(passDesc.execution_type == "2D_IMAGE")
    {
      QRhiTexture* tex = resolveDispatchTexture();
      QSize textureSize = tex ? tex->pixelSize() : QSize(1280, 720);
      dispatchX = (textureSize.width() + localX * strideX - 1) / (localX * strideX);
      dispatchY = (textureSize.height() + localY * strideY - 1) / (localY * strideY);
      dispatchZ = 1;
    }
    else if(passDesc.execution_type == "3D_IMAGE")
    {
      QRhiTexture* tex = resolveDispatchTexture();
      if(tex)
      {
        QSize sz = tex->pixelSize();
        int depth = std::max(1, tex->depth());
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

      if(passDesc.execution_type == "PER_VERTEX"
         || passDesc.execution_type == "PER_INSTANCE")
      {
        const bool per_instance = (passDesc.execution_type == "PER_INSTANCE");
        const std::string& tgt = passDesc.target_resource;
        auto count_of = [per_instance](const auto& b) {
          return per_instance ? b.instance_count : b.vertex_count;
        };

        // Recommended: TARGET names the geometry resource explicitly.
        // Order-independent and self-documenting; should be set on every
        // bundled preset (presets without it fall through to the legacy
        // first-binding-with-positive-count form below).
        bool resolved = false;
        if(!tgt.empty())
        {
          for(const auto& geo_bind : m_geometryBindings)
          {
            if(geo_bind.input_name == tgt)
            {
              const int c = count_of(geo_bind);
              if(c > 0)
              {
                n = c;
                resolved = true;
              }
              break;
            }
          }
          if(!resolved)
          {
            qWarning() << "CSF" << passDesc.execution_type.c_str()
                       << "TARGET" << tgt.c_str()
                       << "not found among geometry bindings, or has zero"
                       << (per_instance ? "instance_count" : "vertex_count");
          }
        }

        // Legacy / TARGET-less fallback: first binding with count > 0.
        if(!resolved)
        {
          for(const auto& geo_bind : m_geometryBindings)
          {
            const int c = count_of(geo_bind);
            if(c > 0)
            {
              n = c;
              break;
            }
          }
        }
      }
      else
      {
        // 1D_BUFFER resolution has three forms, chosen by what the shader
        // author wrote as TARGET:
        //
        //   TARGET = "$expression" or "literal * literal" or "literal":
        //     Treat as an expression. Evaluate through the common resolver
        //     (same variables as SIZE / WIDTH / HEIGHT / STRIDE_*, including
        //     the new $COUNT_<buffer> / $BYTESIZE_<buffer> surface). The
        //     result is the total thread count `n`, which the spreading
        //     logic below distributes across x/y/z workgroups — behaves
        //     like MANUAL but without making the user pick an axis split.
        //
        //   TARGET = "bufferName" (a bare identifier, legacy form):
        //     Dispatch over the buffer's element count. Equivalent to
        //     "$COUNT_bufferName" but kept as shorthand and for backward
        //     compatibility with any existing score that wrote a plain
        //     buffer name.
        //
        //   TARGET empty (no TARGET key in JSON, or empty string):
        //     Fall back to the legacy behaviour — size by the output
        //     storage buffer matching the current edge (in BYTES, which
        //     is a long-standing quirk: dispatches over raw bytes rather
        //     than elements), then by the first geometry's vertex_count.
        //     Left unchanged so existing scores without explicit TARGET
        //     still dispatch the same as before.
        const std::string& target = passDesc.target_resource;

        auto looks_like_expression = [&]() -> bool {
          if(target.empty())
            return false;
          for(char c : target)
          {
            if(c == '$' || c == '+' || c == '-' || c == '*' || c == '/'
               || c == '%' || c == '(' || c == ')')
              return true;
          }
          // Pure integer literal counts as an expression (evaluator's
          // fast-path handles it). Anything else that's a valid identifier
          // character stream is treated as a bare buffer name.
          bool all_numeric = !target.empty();
          for(char c : target)
          {
            if(!std::isdigit((unsigned char)c)
               && !std::isspace((unsigned char)c))
            {
              all_numeric = false;
              break;
            }
          }
          return all_numeric;
        };

        if(looks_like_expression())
        {
          n = resolveDispatchExpression(target);
        }
        else if(!target.empty())
        {
          // Bare buffer name → resolve as "$COUNT_<name>". The common
          // resolver will look it up in m_storageBuffers / auxiliary_ssbos
          // and return the element count. Falls back to 1 on miss.
          const std::string count_expr = "$COUNT_" + target;
          n = resolveDispatchExpression(count_expr);
        }
        else
        {
          // Legacy empty-TARGET fallback — preserved verbatim for
          // compatibility with existing scores.
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
      }

      const auto requiredInvocations = n;
      const auto threadsPerWorkgroup = localX * localY * localZ;
      const int64_t totalWorkgroups = (requiredInvocations + threadsPerWorkgroup * strideX - 1)
                                      / (threadsPerWorkgroup * strideX);
      static constexpr int64_t maxWorkgroups = 65535;

      if(totalWorkgroups > maxWorkgroups * maxWorkgroups * maxWorkgroups)
      {
        // Workgroup count overflow: skip this pass. We haven't yet
        // opened a compute pass at this point (the begin/end for this
        // dispatch is now hoisted *after* the size calculation), so
        // there is nothing to close — just bail to the next pass.
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

    // Guard against dispatch(0,0,0) which is invalid per Vulkan spec.
    // Pass not yet opened, so we just skip without closing anything.
    if(dispatchX <= 0 || dispatchY <= 0 || dispatchZ <= 0)
      continue;

    // Publish the workgroup count to the per-pass ProcessUBO so the
    // shader can read gl_NumWorkGroups via the libisf-injected
    // uniform alias. SPIRV-Cross's HLSL backend cannot emit code for
    // the GLSL NumWorkgroups built-in directly (D3D11/D3D12 bake fails
    // outright), so this routing is what makes compute shaders that
    // reference gl_NumWorkGroups portable across all backends.
    //
    // Must happen before beginComputePass — updateDynamicBuffer is
    // applied as part of the resource update batch that beginComputePass
    // consumes; mid-pass updates are not allowed.
    if(pass.processUBO)
    {
      if(!res)
        res = renderer.state.rhi->nextResourceUpdateBatch();
      n.standardUBO.passIndex = static_cast<int32_t>(passIndex);
      n.standardUBO.numWorkgroups[0] = static_cast<uint32_t>(dispatchX);
      n.standardUBO.numWorkgroups[1] = static_cast<uint32_t>(dispatchY);
      n.standardUBO.numWorkgroups[2] = static_cast<uint32_t>(dispatchZ);
      res->updateDynamicBuffer(
          pass.processUBO, 0, sizeof(ProcessUBO), &n.standardUBO);
    }

    // Begin compute pass with ExternalContent flag so we can insert
    // native memory barriers between dispatches via beginExternal/endExternal.
    commands.beginComputePass(res, QRhiCommandBuffer::BeginPassFlag::ExternalContent);
    res = nullptr;

    commands.setComputePipeline(pass.pipeline);
    commands.setShaderResources(pass.srb);
    commands.dispatch(dispatchX, dispatchY, dispatchZ);

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
        for(auto& aux : gb.auxiliary_ssbos)
        {
          if(aux.access == "read_write" && aux.read_buffer)
            std::swap(aux.buffer, aux.read_buffer);
        }
      }
      gb_idx++;
    }
  }

  // Ping-pong swap for persistent storage images: the primary binding
  // holds the current-frame target, the `_prev` binding reads the
  // previous frame's data. After the frame renders, swap pointers so the
  // next frame reads what we just wrote, and patch every compute SRB
  // that holds these bindings via the indices recorded at build time.
  {
    bool any_swap = false;
    for(auto& si : m_storageImages)
    {
      if(!si.persistent || !si.texture || !si.read_texture)
        continue;
      std::swap(si.texture, si.read_texture);
      si.pending_initial_copy = false;
      any_swap = true;
    }
    if(any_swap)
    {
      for(auto& [e, cp] : m_computePasses)
      {
        if(!cp.srb)
          continue;
        for(const auto& si : m_storageImages)
        {
          if(!si.persistent)
            continue;
          if(si.binding >= 0 && si.texture)
            score::gfx::replaceTexture(*cp.srb, si.binding, si.texture);
          if(si.prev_binding >= 0 && si.read_texture)
            score::gfx::replaceTexture(*cp.srb, si.prev_binding, si.read_texture);
        }
        // No trailing create() — replaceTexture's updateResources() fast
        // path already refreshes the backend descriptor state.
      }

      // Diagnostic 014: graphics passes that visualize the persistent
      // image bake the pre-swap `si.texture` pointer at construction time
      // (createGraphicsPass calls textureForOutput for the edge's source
      // port). After ping-pong, that bound handle now identifies the
      // stale-frame slot. Patch every graphics SRB so it samples the
      // post-swap writable target — i.e. what the next compute dispatch
      // will write into and what we want to display.
      for(auto& [e, gp] : m_graphicsPasses)
      {
        if(!gp.pipeline.srb || !gp.outputSampler)
          continue;
        // Resolve which storage image this graphics pass shows. Mirrors
        // textureForOutput(): first the per-port mapping in
        // m_outStorageImages, otherwise the m_outputTexture fallback.
        QRhiTexture* newTex = nullptr;
        for(const auto& [port, index] : m_outStorageImages)
        {
          if(port == e->source && index < (int)m_storageImages.size())
          {
            const auto& si = m_storageImages[index];
            if(si.persistent)
              newTex = si.texture;
            break;
          }
        }
        if(!newTex)
        {
          // Fallback path — graphics pass uses m_outputTexture. Find the
          // persistent entry whose post-swap read_texture equals the
          // pre-swap m_outputTexture (= what the SRB currently binds).
          for(const auto& si : m_storageImages)
          {
            if(si.persistent && si.read_texture == m_outputTexture)
            {
              newTex = si.texture;
              break;
            }
          }
        }
        if(newTex)
          score::gfx::replaceTexture(*gp.pipeline.srb, gp.outputSampler, newTex);
      }

      // Diagnostic 014: m_outputTexture is the fallback returned by
      // textureForOutput()/resolveDispatchTexture() for default-port
      // queries. It was captured from the first persistent storage
      // image's primary `texture` at build time; after the swap that
      // pointer is the stale-frame slot. Identify the entry whose
      // post-swap read_texture (= pre-swap texture) matches the cached
      // m_outputTexture and refresh it to the new writable target.
      if(m_outputTexture)
      {
        for(const auto& si : m_storageImages)
        {
          if(si.persistent && si.read_texture == m_outputTexture && si.texture)
          {
            m_outputTexture = si.texture;
            break;
          }
        }
      }
    }
  }

  // GENERATE_MIPS: regenerate the mip chain so downstream samplers with a
  // mipmap filter see a valid level > 0. Queued on the same per-frame
  // resource-update batch as the rest of update()'s work — same pattern
  // used for input samplers above at `res.generateMips(texture)`.
  //
  // Gated on FRAMEINDEX > 0: the textures are created with layout
  // PREINITIALIZED and Qt RHI's GenMips path transitions FROM a transfer
  // layout BACK to whatever the texture was stored as. Calling generateMips
  // before the compute pass has actually written the image at least once
  // leaves it in PREINITIALIZED, which trips VUID-VkImageMemoryBarrier-
  // newLayout-01198. After one frame the compute dispatch has transitioned
  // the image to GENERAL and generateMips is safe.
  if(n.standardUBO.frameIndex > 0u)
  {
    for(const auto& si : m_storageImages)
    {
      if(!si.generate_mips || !si.texture)
        continue;
      if(!(si.texture->flags() & QRhiTexture::MipMapped))
        continue;
      res->generateMips(si.texture);
    }
  }
}
}
