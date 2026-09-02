#pragma once

/**
 * @file CaptureAdjustGLSL.hpp
 * @brief The sensor corrections, as shader source shared by every demosaicer.
 *
 * One copy of the maths, because two demosaicers exist for the same frames --
 * a `sampler2D` one for host-staged uploads and a `samplerExternalOES` one for
 * Tegra's imported dma-bufs -- and a correction that differed between them
 * would show up as the picture changing when the capture rung changed, which
 * reads as a capture bug rather than a shader one.
 *
 * Order matters and is not arbitrary:
 *
 *   1. subtract the black level, then renormalise, so the range that survives
 *      still spans 0..1 rather than being compressed toward black;
 *   2. white balance, which is only meaningful once the pedestal is gone --
 *      applying gain to an offset scales the offset too;
 *   3. exposure, a plain linear multiplier;
 *   4. saturation, in linear light where a luma-weighted mix is meaningful;
 *   5. the transfer curve, last, because everything above assumes linear
 *      samples and a curve applied earlier would bend all of it.
 */

namespace score::gfx
{

/// The material block for the capture path: the shared geometry fields, plus
/// the corrections. Matches CaptureMaterialUBO field for field.
#define SCORE_GFX_CAPTURE_UNIFORMS                     \
  "layout(std140, binding = 0) uniform renderer_t {\n" \
  "  mat4 clipSpaceCorrMatrix;\n"                      \
  "  vec2 renderSize;\n"                               \
  "} renderer;\n"                                      \
  "\n"                                                 \
  "layout(std140, binding = 2) uniform material_t {\n" \
  "  vec2 scale;\n"                                    \
  "  vec2 texSz;\n"                                    \
  "  vec4 blackLevel;\n"                               \
  "  vec4 whiteBalance;\n"                             \
  "  vec4 params;\n"                                   \
  "} mat;\n"

/// `adjustCapture(vec3)` -- applies the corrections to linear demosaiced RGB.
///
/// Every step is the identity at its default, so a pipeline that sets nothing
/// gets exactly what it got before this existed.
#define SCORE_GFX_CAPTURE_ADJUST_FN                                            \
  "vec3 adjustCapture(vec3 c)\n"                                               \
  "{\n"                                                                        \
  "  vec3 bl = mat.blackLevel.rgb;\n"                                          \
  "  c = (c - bl) / max(vec3(1.0 / 65535.0), vec3(1.0) - bl);\n"               \
  "  c *= mat.whiteBalance.rgb;\n"                                             \
  "  c *= mat.params.x;\n"                                                     \
  "  c = max(c, vec3(0.0));\n"                                                 \
  "  float l = dot(c, vec3(0.2126, 0.7152, 0.0722));\n"                        \
  "  c = mix(vec3(l), c, mat.params.z);\n"                                     \
  "  c = max(c, vec3(0.0));\n"                                                 \
  "  c = pow(c, vec3(1.0 / max(mat.params.y, 1.0 / 65535.0)));\n"              \
  "  return clamp(c, 0.0, 1.0);\n"                                             \
  "}\n"

/// The vertex shader for a capture decoder.
///
/// Identical to GPUVideoDecoder::vertexShader() except that it declares the
/// long material block. GLSL requires a uniform block of a given name to be
/// declared identically in every stage of a program: giving the fragment stage
/// the corrections while the vertex stage kept the short block made the whole
/// program fail to link -- "struct type mismatch between shaders for uniform
/// (named mat)" -- and a program that does not link draws nothing at all. The
/// vertex stage never reads the corrections; it only has to agree they exist.
static const constexpr auto captureVertexShader = R"_(#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;

layout(location = 0) out vec2 v_texcoord;

)_" SCORE_GFX_CAPTURE_UNIFORMS R"_(

out gl_PerVertex { vec4 gl_Position; };

void main()
{
  v_texcoord = texcoord;
  gl_Position = renderer.clipSpaceCorrMatrix * vec4(position.x * mat.scale.x, position.y * mat.scale.y, 0.0, 1.);
#if defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  gl_Position.y = - gl_Position.y;
#endif
}
)_";


} // namespace score::gfx
