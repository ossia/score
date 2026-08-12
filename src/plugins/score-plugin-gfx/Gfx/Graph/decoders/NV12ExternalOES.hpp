#pragma once

/**
 * @file NV12ExternalOES.hpp
 * @brief NV12 sampled through a GL_TEXTURE_EXTERNAL_OES texture.
 *
 * The zero-copy counterpart to NV12.hpp. Where that decoder takes two plane
 * textures and converts YUV to RGB in the shader, this one takes a single
 * external image and converts nothing: sampling an external texture already
 * returns RGB, because the driver's sampler does the conversion.
 *
 * That is not a preference, it is what EGL drivers implement. Importing NV12
 * as two single-channel 2D images is refused outright -- measured on both
 * Mesa/llvmpipe and Tegra, which reject fourcc 'R8  ' as a 2D texture -- so a
 * dma-buf NV12 frame can only be sampled this way.
 *
 * THE SHADER TRICK
 * ----------------
 * `samplerExternalOES` cannot be baked: glslang rejects the type when
 * targeting SPIR-V, and every score shader goes through QShaderBaker. So the
 * fragment shader is baked normally with `sampler2D` -- giving valid SPIR-V and
 * a correct reflection -- and only its GLSL variant is replaced afterwards with
 * hand-written ESSL declaring `samplerExternalOES`.
 *
 * Nothing has to agree with anything else for this to work: QRhiGles2 takes the
 * bind target from the texture (QGles2Texture::target, set from
 * QRhiTexture::ExternalOES), not from the shader description. So the
 * description keeps saying Sampler2D and the GL program says external, and each
 * is right about its own half.
 */

#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

#include <QFile>

extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{

/// True when this decoder can be used at all: a GLES backend whose driver
/// advertises the external-image extension. Checked before the backend commits
/// to it, because an external texture the driver will not sample is worse than
/// the CPU path.
bool nv12ExternalOesUsable(QRhi::Implementation backend) noexcept;

/// Rewrite baked GLSL so its `sampler2D tex` becomes `samplerExternalOES tex`,
/// adding the required extension pragma. Shared because every external-image
/// decoder needs exactly this edit: glslang rejects the external type, so the
/// shader is baked with sampler2D and only the GL text is swapped. Returns an
/// empty array when the input does not look like baked GLSL.
QByteArray toExternalSamplerEssl(QByteArray glsl);

struct NV12ExternalOESDecoder : GPUVideoDecoder
{
  // Baked with sampler2D so glslang accepts it; the GLSL variant is swapped
  // below. The sampler already returns RGB, so the only work here is the
  // optional user filter.
  static const constexpr auto oes_filter = R"_(#version 450

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

    layout(binding=3) uniform sampler2D tex;

    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;

    vec4 processTexture(vec4 t) {
      vec4 processed = t;
      { %1 }
      return processed;
    }

    void main ()
    {
      fragColor = processTexture(texture(tex, v_texcoord));
    })_";

  explicit NV12ExternalOESDecoder(Video::ImageFormat& d, QString f = "")
      : decoder{d}
      , filter{std::move(f)}
  {
  }

  Video::ImageFormat& decoder;
  QString filter;

  std::pair<QShader, QShader> init(RenderList& r) override;

  /// The strategy owns the external texture and swaps it in; nothing is
  /// uploaded here. exec() exists because a CPU AVFrame cannot be fed through
  /// an external image at all -- if this decoder is ever reached on a CPU path
  /// that is a wiring error, not something to paper over.
  void exec(RenderList&, QRhiResourceUpdateBatch&, AVFrame&) override { }
};

}
