#include "NV12ExternalOES.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <QOpenGLContext>

namespace score::gfx
{

bool nv12ExternalOesUsable(QRhi::Implementation backend) noexcept
{
  if(backend != QRhi::OpenGLES2)
  {
    qDebug() << "NV12-OES: not a GL backend (" << int(backend) << ")";
    return false;
  }
  auto* ctx = QOpenGLContext::currentContext();
  if(!ctx)
  {
    qDebug() << "NV12-OES: no current GL context at probe time";
    return false;
  }
  // essl3 is the variant that works with #version 300 es shaders; the older
  // extension only covers ESSL1. Accept either name, since drivers differ in
  // which they advertise even when both work.
  const bool essl3 = ctx->hasExtension("GL_OES_EGL_image_external_essl3");
  const bool base = ctx->hasExtension("GL_OES_EGL_image_external");
  if(!essl3 && !base)
  {
    // Say what IS advertised. A silent "unsupported" on a driver that plainly
    // implements this is the kind of answer that gets believed and wastes a
    // day; the list settles it in one run.
    QByteArrayList found;
    for(const auto& e : ctx->extensions())
      if(e.contains("image_external") || e.contains("EGL_image"))
        found.push_back(e);
    qDebug() << "NV12-OES: neither external-image extension advertised;"
             << "related extensions present:" << found;
    return false;
  }
  qDebug() << "NV12-OES: external image available (essl3=" << essl3
           << "base=" << base << ")";
  return true;
}

/// Turn the baked GLSL into the external-sampler variant.
///
/// Two surgical edits on the baked text rather than a reconstruction: the
/// baked shader already carries exactly the uniform blocks, locations and
/// precision qualifiers that QRhiGles2 will wire from the reflection, and
/// rebuilding that by hand gets it wrong. (It did: the blocks end `} renderer;`
/// rather than `};`, so slicing to the first `};` produced malformed source and
/// a compile error at token "renderer".)
QByteArray toExternalSamplerEssl(QByteArray glsl)
{
  // 1. the extension pragma, immediately after #version.
  const int verEnd = glsl.indexOf('\n', glsl.indexOf("#version"));
  if(verEnd < 0)
    return {};
  glsl.insert(
      verEnd + 1, "#extension GL_OES_EGL_image_external_essl3 : require\n");

  // 2. the sampler declaration. Whatever qualifiers the baker emitted
  //    (`uniform highp sampler2D tex;` and friends), the whole line becomes the
  //    external form -- an external sampler takes no precision qualifier from
  //    the reflection's point of view and needs none here.
  const int sPos = glsl.indexOf("sampler2D");
  if(sPos < 0)
    return {};
  const int lineStart = glsl.lastIndexOf('\n', sPos) + 1;
  int lineEnd = glsl.indexOf('\n', sPos);
  if(lineEnd < 0)
    lineEnd = glsl.size();
  glsl.replace(
      lineStart, lineEnd - lineStart, "uniform samplerExternalOES tex;");
  return glsl;
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
    const QByteArray src = toExternalSamplerEssl(baked);
    if(src.isEmpty())
    {
      qWarning() << "NV12-OES: could not rewrite the baked GLSL; leaving it "
                    "alone (the rung will render nothing and should decline)";
      continue;
    }
    frag.setShader(key, QShaderCode{src, QByteArrayLiteral("main")});
  }
  return shaders;
}

}
