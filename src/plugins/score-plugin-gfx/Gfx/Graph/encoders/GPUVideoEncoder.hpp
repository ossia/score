#pragma once
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Gfx/Graph/encoders/ColorSpaceOut.hpp>

namespace score::gfx
{

/**
 * @brief Base class for GPU-side video format conversion (RGBA to YUV).
 *
 * This is the inverse of GPUVideoDecoder. Each subclass implements a
 * specific output pixel format (UYVY, NV12, I420, etc.) via a fragment
 * shader that converts RGBA to YUV and writes to one or more textures.
 *
 * The encoder includes Y-flip (GL Y-up -> video Y-down) in its fragment
 * shader, so InvertYRenderer is not needed in the output pipeline.
 * 
 * Readback is also included because there's not going to be a case anytime
 * soon where we can do something useful GPU-side with the Yuv tex.
 *
 * Usage:
 *   1. init() — create textures, samplers, pipeline
 *   2. exec() — render the conversion pass + schedule readback
 *   3. After QRhi::endOffscreenFrame(), call readbackData() to get results
 *   4. release() — free GPU resources
 */
struct GPUVideoEncoder
{
  virtual ~GPUVideoEncoder() = default;

  /// Create render targets, textures, samplers, and the graphics pipeline.
  /// @param rhi The QRhi instance.
  /// @param state The render state (for shader compilation).
  /// @param inputRGBA The scene RGBA texture to read from.
  /// @param width Source width in pixels.
  /// @param height Source height in pixels.
  /// @param colorConversion GLSL code from colorMatrixOut() defining convert_from_rgb().
  ///        If empty, defaults to BT.709 full range.
  virtual void init(
      QRhi& rhi, const RenderState& state, QRhiTexture* inputRGBA, int width, int height,
      const QString& colorConversion = colorMatrixOut())
      = 0;

  /// Execute the conversion pass. Call inside beginOffscreenFrame/endOffscreenFrame.
  /// Renders the conversion shader and schedules GPU->CPU readback(s).
  virtual void exec(QRhi& rhi, QRhiCommandBuffer& cb) = 0;

  /// Number of readback planes (1 for UYVY, 2 for NV12, 3 for I420).
  virtual int planeCount() const = 0;

  /// Get the readback result for a given plane. Valid after endOffscreenFrame.
  virtual const QRhiReadbackResult& readback(int plane) const = 0;

  /// Release all GPU resources.
  virtual void release() = 0;

  /// Vertex shader with hardcoded fullscreen triangle
  /// gl_VertexIndex 0,1,2 -> positions (-1,-1), (3,-1), (-1,3)
  ///                       -> texcoords (0,0), (2,0), (0,2)
  /// Rasterizer clips to [0,1] texcoords over the viewport.
  static constexpr const char* vertex_shader = R"_(#version 450
    layout(location = 0) out vec2 v_texcoord;
    out gl_PerVertex { vec4 gl_Position; };
    void main() {
      vec2 pos = vec2(
        float((gl_VertexIndex & 1) * 4 - 1),
        float((gl_VertexIndex & 2) * 2 - 1)
      );
      v_texcoord = pos * 0.5 + 0.5;
      gl_Position = vec4(pos, 0.0, 1.0);
    }
  )_";

  /// BT.709 full-range RGB->YUV conversion (column-major mat3).
  /// Y  =  0.2126 R + 0.7152 G + 0.0722 B
  /// Cb = -0.1146 R - 0.3854 G + 0.5    B + 0.5
  /// Cr =  0.5    R - 0.4542 G - 0.0458 B + 0.5
  static constexpr const char* rgb_to_yuv_glsl = R"_(
    const vec3 yuv_offset = vec3(0.0, 0.5, 0.5);
    vec3 rgb_to_yuv(vec3 rgb) {
      return vec3(
        dot(rgb, vec3( 0.2126,  0.7152,  0.0722)),
        dot(rgb, vec3(-0.1146, -0.3854,  0.5   )),
        dot(rgb, vec3( 0.5,    -0.4542, -0.0458))
      ) + yuv_offset;
    }
  )_";

  /// Fragment shader Y-flip: on GL, flip v_texcoord.y. On Metal/HLSL, no flip.
  static constexpr const char* y_flip_glsl = R"_(
    vec2 flip_y(vec2 tc) {
    #if defined(QSHADER_MSL) || defined(QSHADER_HLSL)
      return tc;
    #else
      return vec2(tc.x, 1.0 - tc.y);
    #endif
    }
  )_";
};

} // namespace score::gfx
