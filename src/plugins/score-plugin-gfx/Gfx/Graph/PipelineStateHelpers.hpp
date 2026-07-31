#pragma once
#include <isf.hpp>

#include <QtGui/private/qrhi_p.h>

#include <score_plugin_gfx_export.h>

#include <string_view>

namespace score::gfx
{

// --- String → Qt RHI enum mappers ----------------------------------------
//
// All mappers are case-insensitive and accept common synonyms
// (e.g. "lequal" / "less_equal" both map to CompareOp::LessOrEqual).
// Unknown strings fall back to a sensible default (documented per function).

SCORE_PLUGIN_GFX_EXPORT
QRhiGraphicsPipeline::CompareOp toCompareOp(std::string_view s) noexcept;

SCORE_PLUGIN_GFX_EXPORT
QRhiGraphicsPipeline::CullMode toCullMode(std::string_view s) noexcept;

SCORE_PLUGIN_GFX_EXPORT
QRhiGraphicsPipeline::FrontFace toFrontFace(std::string_view s) noexcept;

SCORE_PLUGIN_GFX_EXPORT
QRhiGraphicsPipeline::PolygonMode toPolygonMode(std::string_view s) noexcept;

SCORE_PLUGIN_GFX_EXPORT
QRhiGraphicsPipeline::BlendFactor toBlendFactor(std::string_view s) noexcept;

SCORE_PLUGIN_GFX_EXPORT
QRhiGraphicsPipeline::BlendOp toBlendOp(std::string_view s) noexcept;

SCORE_PLUGIN_GFX_EXPORT
QRhiGraphicsPipeline::StencilOp toStencilOp(std::string_view s) noexcept;

SCORE_PLUGIN_GFX_EXPORT
QRhiGraphicsPipeline::ColorMask toColorMask(std::string_view s) noexcept;

// --- Conversion helpers ---------------------------------------------------

SCORE_PLUGIN_GFX_EXPORT
QRhiGraphicsPipeline::TargetBlend toTargetBlend(const isf::blend_attachment& b) noexcept;

SCORE_PLUGIN_GFX_EXPORT
QRhiGraphicsPipeline::StencilOpState toStencilOpState(const isf::stencil_op_state& s) noexcept;

// --- pipeline_state manipulation ------------------------------------------

// Merge two pipeline_states: every field that is set in `over` wins, otherwise
// `base`'s field is kept. Used to combine the descriptor's global state with a
// per-pass override_state.
SCORE_PLUGIN_GFX_EXPORT
isf::pipeline_state mergeState(isf::pipeline_state base, const isf::pipeline_state& over);

// Returns true if the state has any field set (i.e. would affect a pipeline).
SCORE_PLUGIN_GFX_EXPORT
bool stateAffectsPipeline(const isf::pipeline_state&) noexcept;

// Apply the state to a graphics pipeline.
// - `colorAttachmentCount`: used to size per-attachment blend vectors.
// - `depthAttachmentAvailable`: true when the target RT has a depth attachment;
//   depth-test/write are forced off otherwise.
// - `wantsDepthByDefault`: legacy fallback. When state.depth_test is nullopt
//   AND wantsDepthByDefault is false, depth test/write are force-disabled
//   (equivalent to today's `!renderer.anyNodeRequiresDepth()` path).
//
// Only fields explicitly set in `state` are overridden. Cull, front-face,
// polygon mode, blend, and stencil all preserve whatever the caller (or
// `mesh.preparePipeline()`) configured before this call. The caller is
// responsible for seeding sensible defaults (e.g. premul-alpha blend) before
// invoking this, so that shaders declaring partial pipeline_state don't
// silently lose unrelated defaults.
SCORE_PLUGIN_GFX_EXPORT
void applyPipelineState(
    QRhiGraphicsPipeline& pip,
    const isf::pipeline_state& state,
    int colorAttachmentCount,
    bool depthAttachmentAvailable,
    bool wantsDepthByDefault) noexcept;

}
