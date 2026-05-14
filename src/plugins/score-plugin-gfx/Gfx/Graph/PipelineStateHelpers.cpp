#include "PipelineStateHelpers.hpp"

#include <algorithm>
#include <cctype>

namespace
{
// Case-insensitive comparison: "lessOrEqual" == "less_or_equal" == "LEQUAL".
// Strips underscores/hyphens so all forms compare equal.
static bool ieq(std::string_view a, const char* b)
{
  std::size_t bi = 0;
  for(std::size_t i = 0; i < a.size(); ++i)
  {
    char ca = (char)std::tolower((unsigned char)a[i]);
    if(ca == '_' || ca == '-' || ca == ' ')
      continue;
    if(b[bi] == '\0')
      return false;
    char cb = (char)std::tolower((unsigned char)b[bi]);
    if(ca != cb)
      return false;
    ++bi;
  }
  return b[bi] == '\0';
}
}

namespace score::gfx
{

QRhiGraphicsPipeline::CompareOp toCompareOp(std::string_view s) noexcept
{
  if(ieq(s, "never"))          return QRhiGraphicsPipeline::Never;
  if(ieq(s, "less") || ieq(s, "l"))  return QRhiGraphicsPipeline::Less;
  if(ieq(s, "equal") || ieq(s, "eq")) return QRhiGraphicsPipeline::Equal;
  if(ieq(s, "lessorequal") || ieq(s, "lessequal") || ieq(s, "lequal"))
    return QRhiGraphicsPipeline::LessOrEqual;
  if(ieq(s, "greater") || ieq(s, "g") || ieq(s, "gt"))
    return QRhiGraphicsPipeline::Greater;
  if(ieq(s, "notequal") || ieq(s, "neq") || ieq(s, "ne"))
    return QRhiGraphicsPipeline::NotEqual;
  if(ieq(s, "greaterorequal") || ieq(s, "greaterequal") || ieq(s, "gequal"))
    return QRhiGraphicsPipeline::GreaterOrEqual;
  if(ieq(s, "always"))         return QRhiGraphicsPipeline::Always;
  return QRhiGraphicsPipeline::Less;
}

QRhiGraphicsPipeline::CullMode toCullMode(std::string_view s) noexcept
{
  if(ieq(s, "none"))  return QRhiGraphicsPipeline::None;
  if(ieq(s, "front")) return QRhiGraphicsPipeline::Front;
  if(ieq(s, "back"))  return QRhiGraphicsPipeline::Back;
  return QRhiGraphicsPipeline::None;
}

QRhiGraphicsPipeline::FrontFace toFrontFace(std::string_view s) noexcept
{
  if(ieq(s, "ccw") || ieq(s, "counterclockwise"))
    return QRhiGraphicsPipeline::CCW;
  if(ieq(s, "cw") || ieq(s, "clockwise"))
    return QRhiGraphicsPipeline::CW;
  return QRhiGraphicsPipeline::CCW;
}

QRhiGraphicsPipeline::PolygonMode toPolygonMode(std::string_view s) noexcept
{
  if(ieq(s, "fill") || ieq(s, "solid"))     return QRhiGraphicsPipeline::Fill;
  if(ieq(s, "line") || ieq(s, "wireframe")) return QRhiGraphicsPipeline::Line;
  return QRhiGraphicsPipeline::Fill;
}

QRhiGraphicsPipeline::Topology toTopology(std::string_view s) noexcept
{
  if(ieq(s, "triangles") || ieq(s, "triangle_list"))
    return QRhiGraphicsPipeline::Triangles;
  if(ieq(s, "triangle_strip")) return QRhiGraphicsPipeline::TriangleStrip;
  if(ieq(s, "triangle_fan"))   return QRhiGraphicsPipeline::TriangleFan;
  if(ieq(s, "lines") || ieq(s, "line_list"))
    return QRhiGraphicsPipeline::Lines;
  if(ieq(s, "line_strip"))     return QRhiGraphicsPipeline::LineStrip;
  if(ieq(s, "points"))         return QRhiGraphicsPipeline::Points;
  return QRhiGraphicsPipeline::Triangles;
}

QRhiGraphicsPipeline::BlendFactor toBlendFactor(std::string_view s) noexcept
{
  using B = QRhiGraphicsPipeline;
  if(ieq(s, "zero"))                  return B::Zero;
  if(ieq(s, "one"))                   return B::One;
  if(ieq(s, "srccolor"))              return B::SrcColor;
  if(ieq(s, "oneminussrccolor") || ieq(s, "1-srccolor")) return B::OneMinusSrcColor;
  if(ieq(s, "dstcolor"))              return B::DstColor;
  if(ieq(s, "oneminusdstcolor") || ieq(s, "1-dstcolor")) return B::OneMinusDstColor;
  if(ieq(s, "srcalpha"))              return B::SrcAlpha;
  if(ieq(s, "oneminussrcalpha") || ieq(s, "1-srcalpha")) return B::OneMinusSrcAlpha;
  if(ieq(s, "dstalpha"))              return B::DstAlpha;
  if(ieq(s, "oneminusdstalpha") || ieq(s, "1-dstalpha")) return B::OneMinusDstAlpha;
  if(ieq(s, "constantcolor"))         return B::ConstantColor;
  if(ieq(s, "oneminusconstantcolor") || ieq(s, "1-constantcolor")) return B::OneMinusConstantColor;
  if(ieq(s, "constantalpha"))         return B::ConstantAlpha;
  if(ieq(s, "oneminusconstantalpha") || ieq(s, "1-constantalpha")) return B::OneMinusConstantAlpha;
  if(ieq(s, "srcalphasaturate"))      return B::SrcAlphaSaturate;
  if(ieq(s, "src1color"))             return B::Src1Color;
  if(ieq(s, "oneminussrc1color"))     return B::OneMinusSrc1Color;
  if(ieq(s, "src1alpha"))             return B::Src1Alpha;
  if(ieq(s, "oneminussrc1alpha"))     return B::OneMinusSrc1Alpha;
  return B::One;
}

QRhiGraphicsPipeline::BlendOp toBlendOp(std::string_view s) noexcept
{
  using B = QRhiGraphicsPipeline;
  if(ieq(s, "add"))             return B::Add;
  if(ieq(s, "subtract") || ieq(s, "sub")) return B::Subtract;
  if(ieq(s, "reversesubtract") || ieq(s, "revsub")) return B::ReverseSubtract;
  if(ieq(s, "min"))             return B::Min;
  if(ieq(s, "max"))             return B::Max;
  return B::Add;
}

QRhiGraphicsPipeline::StencilOp toStencilOp(std::string_view s) noexcept
{
  using S = QRhiGraphicsPipeline;
  if(ieq(s, "zero"))              return S::StencilZero;
  if(ieq(s, "keep"))              return S::Keep;
  if(ieq(s, "replace"))           return S::Replace;
  if(ieq(s, "incrementandclamp") || ieq(s, "incclamp") || ieq(s, "increment"))
    return S::IncrementAndClamp;
  if(ieq(s, "decrementandclamp") || ieq(s, "decclamp") || ieq(s, "decrement"))
    return S::DecrementAndClamp;
  if(ieq(s, "invert"))            return S::Invert;
  if(ieq(s, "incrementandwrap") || ieq(s, "incwrap"))
    return S::IncrementAndWrap;
  if(ieq(s, "decrementandwrap") || ieq(s, "decwrap"))
    return S::DecrementAndWrap;
  return S::Keep;
}

QRhiGraphicsPipeline::ColorMask toColorMask(std::string_view s) noexcept
{
  using M = QRhiGraphicsPipeline;
  M::ColorMask out = M::ColorMask(0);
  for(char c : s)
  {
    switch(std::tolower((unsigned char)c))
    {
      case 'r': out |= M::R; break;
      case 'g': out |= M::G; break;
      case 'b': out |= M::B; break;
      case 'a': out |= M::A; break;
      default: break;
    }
  }
  if(out == M::ColorMask(0))
    out = M::R | M::G | M::B | M::A;
  return out;
}

QRhiGraphicsPipeline::TargetBlend toTargetBlend(const isf::blend_attachment& b) noexcept
{
  QRhiGraphicsPipeline::TargetBlend out;
  out.enable = b.enable;
  out.srcColor = toBlendFactor(b.src_color);
  out.dstColor = toBlendFactor(b.dst_color);
  out.opColor  = toBlendOp(b.op_color);
  out.srcAlpha = toBlendFactor(b.src_alpha);
  out.dstAlpha = toBlendFactor(b.dst_alpha);
  out.opAlpha  = toBlendOp(b.op_alpha);
  out.colorWrite = toColorMask(b.color_write);
  return out;
}

QRhiGraphicsPipeline::StencilOpState toStencilOpState(const isf::stencil_op_state& s) noexcept
{
  QRhiGraphicsPipeline::StencilOpState out;
  out.failOp      = toStencilOp(s.fail_op);
  out.depthFailOp = toStencilOp(s.depth_fail_op);
  out.passOp      = toStencilOp(s.pass_op);
  out.compareOp   = toCompareOp(s.compare_op);
  return out;
}

// --- pipeline_state manipulation ------------------------------------------

isf::pipeline_state mergeState(isf::pipeline_state base, const isf::pipeline_state& over)
{
  if(over.depth_test.has_value())             base.depth_test = over.depth_test;
  if(over.depth_write.has_value())            base.depth_write = over.depth_write;
  if(over.depth_compare.has_value())          base.depth_compare = over.depth_compare;
  if(over.depth_bias.has_value())             base.depth_bias = over.depth_bias;
  if(over.slope_scaled_depth_bias.has_value())base.slope_scaled_depth_bias = over.slope_scaled_depth_bias;
  if(over.cull_mode.has_value())              base.cull_mode = over.cull_mode;
  if(over.front_face.has_value())             base.front_face = over.front_face;
  if(over.polygon_mode.has_value())           base.polygon_mode = over.polygon_mode;
  if(over.line_width.has_value())             base.line_width = over.line_width;
  if(over.vertex_count.has_value())           base.vertex_count = over.vertex_count;
  if(over.instance_count.has_value())         base.instance_count = over.instance_count;
  if(over.topology.has_value())               base.topology = over.topology;
  if(over.blend_all.has_value())              base.blend_all = over.blend_all;
  if(!over.blend_per_attachment.empty())      base.blend_per_attachment = over.blend_per_attachment;
  if(over.stencil_test.has_value())           base.stencil_test = over.stencil_test;
  if(over.stencil_read_mask.has_value())      base.stencil_read_mask = over.stencil_read_mask;
  if(over.stencil_write_mask.has_value())     base.stencil_write_mask = over.stencil_write_mask;
  if(over.stencil_front.has_value())          base.stencil_front = over.stencil_front;
  if(over.stencil_back.has_value())           base.stencil_back = over.stencil_back;
  return base;
}

bool stateAffectsPipeline(const isf::pipeline_state& s) noexcept
{
  return s.depth_test.has_value()
      || s.depth_write.has_value()
      || s.depth_compare.has_value()
      || s.depth_bias.has_value()
      || s.slope_scaled_depth_bias.has_value()
      || s.cull_mode.has_value()
      || s.front_face.has_value()
      || s.polygon_mode.has_value()
      || s.line_width.has_value()
      || s.blend_all.has_value()
      || !s.blend_per_attachment.empty()
      || s.stencil_test.has_value()
      || s.stencil_read_mask.has_value()
      || s.stencil_write_mask.has_value()
      || s.stencil_front.has_value()
      || s.stencil_back.has_value()
      || s.topology.has_value()
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
      || s.shading_rate.has_value()
#endif
      ;
  // vertex_count / instance_count don't affect the pipeline itself
  // (they change draw arguments, not pipeline state), so they're
  // intentionally absent from this check.
}

void applyPipelineState(
    QRhiGraphicsPipeline& pip,
    const isf::pipeline_state& state,
    int colorAttachmentCount,
    bool depthAttachmentAvailable,
    bool wantsDepthByDefault) noexcept
{
  // ── Depth ──────────────────────────────────────────────────────────
  // Only override depth state when explicitly set, OR when we need to force
  // it off (no depth attachment, or upstream doesn't require depth). This
  // preserves whatever the caller / mesh.preparePipeline already configured.
  if(state.depth_test.has_value())
  {
    pip.setDepthTest(depthAttachmentAvailable && *state.depth_test);
  }
  else if(!depthAttachmentAvailable || !wantsDepthByDefault)
  {
    pip.setDepthTest(false);
  }

  if(state.depth_write.has_value())
  {
    pip.setDepthWrite(depthAttachmentAvailable && *state.depth_write);
  }
  else if(!depthAttachmentAvailable || !wantsDepthByDefault)
  {
    pip.setDepthWrite(false);
  }

  // Reverse-Z project rule: when depth is enabled and the shader didn't
  // pick a compare op explicitly, default to Greater (near → 1.0, far →
  // 0.0 in the float depth buffer). QRhi's built-in default is Less, which
  // rejects every fragment under reverse-Z conventions.
  if(state.depth_compare.has_value())
    pip.setDepthOp(toCompareOp(*state.depth_compare));
  else
    pip.setDepthOp(QRhiGraphicsPipeline::Greater);
  if(state.depth_bias.has_value())
    pip.setDepthBias((int)*state.depth_bias);
  if(state.slope_scaled_depth_bias.has_value())
    pip.setSlopeScaledDepthBias(*state.slope_scaled_depth_bias);

  // ── Cull / front-face / polygon mode ────────────────────────────────
  // Only override when explicitly set; else preserve the caller's setup.
  if(state.cull_mode.has_value())
    pip.setCullMode(toCullMode(*state.cull_mode));

  if(state.front_face.has_value())
    pip.setFrontFace(toFrontFace(*state.front_face));

  if(state.polygon_mode.has_value())
    pip.setPolygonMode(toPolygonMode(*state.polygon_mode));

  if(state.line_width.has_value())
    pip.setLineWidth(*state.line_width);

  // Topology override (paired with vertex_count for procedural draws):
  // lets a shader that uses VERTEX_COUNT emit points / lines / strips
  // without depending on the incoming geometry's topology.
  if(state.topology.has_value())
    pip.setTopology(toTopology(*state.topology));

  // ── Blending ────────────────────────────────────────────────────────
  // Only override target blends when the shader explicitly declares blend
  // state. Otherwise the caller's seeded blend (e.g. legacy premul-alpha)
  // is preserved bit-exact.
  const int nAttachments = std::max(1, colorAttachmentCount);
  if(!state.blend_per_attachment.empty())
  {
    QVarLengthArray<QRhiGraphicsPipeline::TargetBlend, 4> blends;
    blends.reserve(nAttachments);
    for(int i = 0; i < nAttachments; ++i)
    {
      std::size_t idx = std::min<std::size_t>(i, state.blend_per_attachment.size() - 1);
      blends.push_back(toTargetBlend(state.blend_per_attachment[idx]));
    }
    pip.setTargetBlends(blends.begin(), blends.end());
  }
  else if(state.blend_all.has_value())
  {
    QVarLengthArray<QRhiGraphicsPipeline::TargetBlend, 4> blends;
    blends.reserve(nAttachments);
    auto t = toTargetBlend(*state.blend_all);
    for(int i = 0; i < nAttachments; ++i)
      blends.push_back(t);
    pip.setTargetBlends(blends.begin(), blends.end());
  }

  // ── Stencil ─────────────────────────────────────────────────────────
  // Toggle is gated on `stencil_test` only; sub-fields apply
  // independently so a shader can override e.g. front op without
  // re-stating `stencil_test`.
  if(state.stencil_test.has_value())
    pip.setStencilTest(*state.stencil_test);
  if(state.stencil_front.has_value())
    pip.setStencilFront(toStencilOpState(*state.stencil_front));
  if(state.stencil_back.has_value())
    pip.setStencilBack(toStencilOpState(*state.stencil_back));
  if(state.stencil_read_mask.has_value())
    pip.setStencilReadMask(*state.stencil_read_mask);
  if(state.stencil_write_mask.has_value())
    pip.setStencilWriteMask(*state.stencil_write_mask);

  // ── Variable-rate shading (per-draw rate) ───────────────────────────
  // QRhiGraphicsPipeline::setShadingRate expects a ShadingRate enum value
  // encoded as width/height pair. VRS is only honoured on backends that
  // advertise QRhi::Feature::VariableRateShading; calling the setter on
  // other backends is a no-op.
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
  if(state.shading_rate.has_value())
  {
    const auto& sr = *state.shading_rate;
    auto clamp_rate = [](int v) {
      if(v >= 4) return 4;
      if(v >= 2) return 2;
      return 1;
    };
    const int w = clamp_rate(sr[0]);
    const int h = clamp_rate(sr[1]);
    // QRhi encodes the shading rate as a small enum; we build it here from
    // the requested w,h pair. The encoding follows Vulkan's
    // VkFragmentShadingRateNV / VK_KHR_fragment_shading_rate convention:
    // log2(w) << 2 | log2(h).
    int rateEnum = 0;
    switch(w) { case 2: rateEnum |= (1 << 2); break; case 4: rateEnum |= (2 << 2); break; }
    switch(h) { case 2: rateEnum |= 1; break; case 4: rateEnum |= 2; break; }
    pip.setShadingRate(static_cast<QRhiGraphicsPipeline::ShadingRate>(rateEnum));
  }
#endif
}

}
