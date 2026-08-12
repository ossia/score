#include "NV12ExternalOES.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <QOpenGLContext>

namespace score::gfx
{

bool nv12ExternalOesUsable(QRhi::Implementation backend) noexcept
{
  if(backend != QRhi::OpenGLES2)
    return false;
  auto* ctx = QOpenGLContext::currentContext();
  if(!ctx)
    return false;
  // essl3 is the variant that works with #version 300 es shaders; the older
  // extension only covers ESSL1. Accept either name, since drivers differ in
  // which they advertise even when both work.
  return ctx->hasExtension("GL_OES_EGL_image_external_essl3")
         || ctx->hasExtension("GL_OES_EGL_image_external");
}

namespace
{
/// Hand-written ESSL for the external sampler, substituted for the baked GLSL.
///
/// It must match what the baked shader's reflection describes -- same binding,
/// same in/out locations, same uniform block -- because QRhiGles2 wires the
/// program from that reflection. Only the sampler's *type* differs, and that is
/// the whole point: the type cannot survive a SPIR-V round trip.
QByteArray externalEssl(const QByteArray& bakedGlsl, const QString& filter)
{
  QByteArray out;
  out += "#version 300 es\n";
  out += "#extension GL_OES_EGL_image_external_essl3 : require\n";
  out += "precision mediump float;\n";
  // The baked GLSL carries the uniform block the reflection expects; reusing
  // its declaration verbatim keeps the two in step if the shared uniform
  // header ever changes.
  const int ubStart = bakedGlsl.indexOf("uniform");
  const int ubEnd = bakedGlsl.indexOf("};", ubStart);
  if(ubStart >= 0 && ubEnd > ubStart)
  {
    out += bakedGlsl.mid(ubStart, ubEnd - ubStart + 2);
    out += "\n";
  }
  out += "uniform samplerExternalOES tex;\n";
  out += "in vec2 v_texcoord;\n";
  out += "out vec4 fragColor;\n";
  out += "vec4 processTexture(vec4 t) {\n  vec4 processed = t;\n  {";
  out += filter.toUtf8();
  out += "}\n  return processed;\n}\n";
  out += "void main() {\n  fragColor = processTexture(texture(tex, v_texcoord));\n}\n";
  return out;
}
}

std::pair<QShader, QShader> NV12ExternalOESDecoder::init(RenderList& r)
{
  auto& rhi = *r.state.rhi;
  const auto w = decoder.width, h = decoder.height;

  {
    // One texture for the whole frame. ExternalOES is what makes QRhiGles2 use
    // GL_TEXTURE_EXTERNAL_OES as the bind target; the format is nominal, since
    // the external image defines its own.
    auto tex = rhi.newTexture(
        QRhiTexture::RGBA8, QSize{w, h}, 1, QRhiTexture::ExternalOES);
    tex->create();

    auto sampler = rhi.newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    sampler->create();

    samplers.push_back({sampler, tex});
  }

  auto shaders = score::gfx::makeShaders(
      r.state, vertexShader(), QString(oes_filter).arg(filter));

  // Swap the GLSL variant for one declaring samplerExternalOES. The SPIR-V and
  // the reflection stay as baked -- only the text the GL backend compiles
  // changes, which is the one place the type is allowed to appear.
  QShader& frag = shaders.second;
  for(const auto& key : frag.availableShaders())
  {
    if(key.source() != QShader::GlslShader)
      continue;
    const QByteArray baked = frag.shader(key).shader();
    frag.setShader(
        key, QShaderCode{externalEssl(baked, filter), QByteArrayLiteral("main")});
  }
  return shaders;
}

}
