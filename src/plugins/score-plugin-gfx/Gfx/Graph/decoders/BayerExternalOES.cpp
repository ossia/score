#include "BayerExternalOES.hpp"

#include "NV12ExternalOES.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <QDebug>

namespace score::gfx
{

std::pair<QShader, QShader> BayerExternalOESDecoder::init(RenderList& r)
{
  auto& rhi = *r.state.rhi;
  const auto w = decoder.width, h = decoder.height;

  {
    // ExternalOES is what makes QRhiGles2 bind GL_TEXTURE_EXTERNAL_OES; the
    // format is nominal, since the imported image carries its own.
    auto tex = rhi.newTexture(
        QRhiTexture::RGBA8, QSize{w, h}, 1, QRhiTexture::ExternalOES);
    tex->create();

    // Nearest, not preference: a mosaic reconstructed from LINEAR taps has its
    // colour sites blended together before the demosaic can separate them.
    auto sampler = rhi.newSampler(
        QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    sampler->create();

    samplers.push_back({sampler, tex});
  }

  int px = 0, py = 0;
  switch(phase)
  {
    case BayerDecoder::Phase::RGGB: px = 0; py = 0; break;
    case BayerDecoder::Phase::GRBG: px = 1; py = 0; break;
    case BayerDecoder::Phase::GBRG: px = 0; py = 1; break;
    case BayerDecoder::Phase::BGGR: px = 1; py = 1; break;
  }

  auto shaders = score::gfx::makeShaders(
      r.state, score::gfx::captureVertexShader,
      QString(oes_filter)
          .arg(px)
          .arg(py)
          .arg(QString::number(sampleScale, 'f', 6)));

  // Swap the GLSL the GL backend compiles for one declaring samplerExternalOES.
  // The SPIR-V and the reflection stay as baked: QRhiGles2 takes the bind
  // target from the texture, not from the shader description, so each half is
  // right about its own side.
  QShader& frag = shaders.second;
  for(const auto& key : frag.availableShaders())
  {
    if(key.source() != QShader::GlslShader)
      continue;
    const QByteArray baked = frag.shader(key).shader();
    const QByteArray src = toExternalSamplerEssl(baked);
    if(src.isEmpty())
    {
      qWarning() << "Bayer-OES: could not rewrite the baked GLSL; leaving it "
                    "alone (the rung will render nothing and should decline)";
      continue;
    }
    frag.setShader(key, QShaderCode{src, QByteArrayLiteral("main")});
  }
  return shaders;
}

}
