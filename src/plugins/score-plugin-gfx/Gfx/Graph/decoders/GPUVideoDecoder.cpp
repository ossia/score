#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

namespace score::gfx
{

GPUVideoDecoder::GPUVideoDecoder() { }

GPUVideoDecoder::~GPUVideoDecoder() { }

void GPUVideoDecoder::release(RenderList&)
{
  for(auto [sampler, tex] : samplers)
    tex->deleteLater();

  for(auto sampler : samplers)
  {
    delete sampler.sampler;
  }
  samplers.clear();
}

QRhiTextureSubresourceUploadDescription GPUVideoDecoder::createTextureUpload(
    uint8_t* pixels, int w, int h, int bytesPerPixel, int stride)
{
  QRhiTextureSubresourceUploadDescription subdesc;

  const int rowBytes = w * bytesPerPixel;
  if(rowBytes == stride)
  {
    subdesc.setData(
        QByteArray::fromRawData(reinterpret_cast<const char*>(pixels), rowBytes * h));
  }
  else
  {
    QByteArray data(w * h * bytesPerPixel, Qt::Uninitialized);
    for(int r = 0; r < h; r++)
    {
      const char* row = reinterpret_cast<const char*>(pixels + stride * r);
      char* output = data.data() + rowBytes * r;
      std::copy(row, row + rowBytes, output);
    }
    subdesc.setData(std::move(data));
  }

  return subdesc;
}

QString GPUVideoDecoder::vertexShader() noexcept
{
  static constexpr const char* shader = R"_(#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;

layout(location = 0) out vec2 v_texcoord;

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

out gl_PerVertex { vec4 gl_Position; };

void main()
{
  v_texcoord = texcoord;
  gl_Position = renderer.clipSpaceCorrMatrix * vec4(position.x * mat.scale.x, position.y * mat.scale.y, 0.0, 1.);
#if defined(QSHADER_HLSL)
  gl_Position.y = - gl_Position.y;
#endif
}
)_";

  return shader;
}

}
