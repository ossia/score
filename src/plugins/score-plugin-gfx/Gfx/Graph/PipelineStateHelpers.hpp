#pragma once
#include <isf.hpp>

#include <QtGui/private/qrhi_p.h>

#include <score_plugin_gfx_export.h>

#include <string_view>

namespace score::gfx
{
class Node;
struct Port;

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

// Fill when `requested` is not Fill and the backend does not report
// QRhi::NonFillPolygonMode; warns once per process in that case.
SCORE_PLUGIN_GFX_EXPORT
QRhiGraphicsPipeline::PolygonMode supportedPolygonMode(
    QRhiGraphicsPipeline::PolygonMode requested, bool nonFillSupported) noexcept;

SCORE_PLUGIN_GFX_EXPORT
QRhiGraphicsPipeline::BlendFactor toBlendFactor(std::string_view s) noexcept;

SCORE_PLUGIN_GFX_EXPORT
QRhiGraphicsPipeline::BlendOp toBlendOp(std::string_view s) noexcept;

SCORE_PLUGIN_GFX_EXPORT
QRhiGraphicsPipeline::StencilOp toStencilOp(std::string_view s) noexcept;

SCORE_PLUGIN_GFX_EXPORT
QRhiGraphicsPipeline::ColorMask toColorMask(std::string_view s) noexcept;

// Depth-attachment clear value to pair with a declared DEPTH_COMPARE.
//
// Depth after the viewport transform is in [0, 1], so a clear value only
// admits fragments on one side of it: 0.0 works with `greater` / `greater_equal`
// (the project-wide reverse-Z convention documented in CameraMath.hpp) and
// rejects every fragment under `less` / `less_equal`.
SCORE_PLUGIN_GFX_EXPORT
float depthClearForCompare(QRhiGraphicsPipeline::CompareOp compare) noexcept;

// The same, for a shader's declared state: uses its DEPTH_COMPARE when it set
// one, and otherwise the project-wide reverse-Z default.
//
// Every path that opens a pass with a depth attachment must go through this.
// Picking the clear independently of the compare is not a cosmetic mismatch --
// it silently discards the whole draw, because a clear of 0.0 admits nothing
// under `less`.
SCORE_PLUGIN_GFX_EXPORT
float depthClearForState(const isf::pipeline_state& state) noexcept;

// --- Conversion helpers ---------------------------------------------------

SCORE_PLUGIN_GFX_EXPORT
QRhiGraphicsPipeline::TargetBlend toTargetBlend(const isf::blend_attachment& b) noexcept;

SCORE_PLUGIN_GFX_EXPORT
QRhiGraphicsPipeline::StencilOpState toStencilOpState(const isf::stencil_op_state& s) noexcept;

// --- Output compositing ---------------------------------------------------
//
// Render targets between nodes hold premultiplied colour. A shader output is
// composited "over" what its target already holds, with the factors that
// match what it writes (its ALPHA key); both store premultiplied colour.

// Colour One / OneMinusSrcAlpha, alpha One / OneMinusSrcAlpha.
SCORE_PLUGIN_GFX_EXPORT
QRhiGraphicsPipeline::TargetBlend premultipliedOverBlend() noexcept;

// Colour SrcAlpha / OneMinusSrcAlpha, alpha One / OneMinusSrcAlpha.
SCORE_PLUGIN_GFX_EXPORT
QRhiGraphicsPipeline::TargetBlend straightOverBlend() noexcept;

// The blend compositing an output onto its target, from its ALPHA and
// COMPOSITE. The destination holds premultiplied colour. A straight output
// with multiply or screen gets the premultiplied factors (it is premultiplied
// before blending, see isf::premultiplied_by_engine).
//   over     : colour src / OneMinusSrcAlpha, alpha One / OneMinusSrcAlpha
//   add      : colour src / One,              alpha One / One
//   multiply : colour DstColor / OneMinusSrcAlpha (exact over an opaque
//              destination), alpha One / OneMinusSrcAlpha
//   screen   : colour OneMinusDstColor / One, alpha One / OneMinusSrcAlpha
//   replace  : colour src / Zero,             alpha One / Zero
// where src is SrcAlpha for straight and One for premultiplied.
SCORE_PLUGIN_GFX_EXPORT
QRhiGraphicsPipeline::TargetBlend
blendFor(isf::alpha_mode alpha, isf::composite_mode composite) noexcept;

// The blend of an output into the node's own cleared texture, which is then
// copied into its consumers with blendFor(premultiplied, composite): as
// blendFor, except multiply, which composites over (a layer multiplied onto
// transparent black would vanish).
SCORE_PLUGIN_GFX_EXPORT
QRhiGraphicsPipeline::TargetBlend
layerBlendFor(isf::alpha_mode alpha, isf::composite_mode composite) noexcept;

// One blend per colour attachment: attachment i takes the ALPHA and COMPOSITE
// of the i-th colour OUTPUT (depth OUTPUTS skipped), else the descriptor's.
// Integer FORMATs get no blend. ownTargets: the node renders into its own
// textures (layerBlendFor) rather than into its consumers.
SCORE_PLUGIN_GFX_EXPORT
QVarLengthArray<QRhiGraphicsPipeline::TargetBlend, 4> outputBlends(
    const isf::descriptor& desc, int colorAttachmentCount, bool ownTargets = false);

// The blend copying an output that the node stored premultiplied in its own
// texture into a consumer: its COMPOSITE, or over when the shader declares
// BLEND.
SCORE_PLUGIN_GFX_EXPORT
QRhiGraphicsPipeline::TargetBlend
copyBlendFor(const isf::descriptor& desc, const isf::output_declaration* out) noexcept;

// The OUTPUTS entry of a colour image outlet of an ISF / raw raster node
// (image outlets only, in OUTPUTS order), or nullptr.
SCORE_PLUGIN_GFX_EXPORT
const isf::output_declaration*
colorOutputDeclaration(const isf::descriptor& desc, const Node& node, const Port& output);

// Integer colour formats cannot be blended.
SCORE_PLUGIN_GFX_EXPORT
bool formatSupportsBlending(QRhiTexture::Format f) noexcept;

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
// - `wantsDepthByDefault`: when state.depth_test is nullopt and this is false,
//   depth test/write are force-disabled; callers pass
//   `renderer.anyNodeRequiresDepth()`.
//
// Only fields explicitly set in `state` are overridden. Cull, front-face,
// polygon mode, blend, and stencil all preserve whatever the caller (or
// `mesh.preparePipeline()`) configured before this call. The caller is
// responsible for seeding sensible defaults (e.g. blendFor(ALPHA, COMPOSITE)) before
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
