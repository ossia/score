#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

namespace score::gfx
{

GPUVideoDecoder::GPUVideoDecoder()
{

}

GPUVideoDecoder::~GPUVideoDecoder() { }

void GPUVideoDecoder::release(RenderList&)
{
#include <Gfx/Qt5CompatPush> // clang-format: keep

  for (auto [sampler, tex] : samplers)
    tex->deleteLater();

#include <Gfx/Qt5CompatPop> // clang-format: keep

  for (auto sampler : samplers)
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
  if (rowBytes == stride)
  {
    subdesc.setData(QByteArray::fromRawData(
        reinterpret_cast<const char*>(pixels), rowBytes * h));
  }
  else
  {
    QByteArray data{w * h, Qt::Uninitialized};
    for (int r = 0; r < h; r++)
    {
      const char* input = reinterpret_cast<const char*>(pixels + stride * r);
      char* output = data.data() + rowBytes * r;
      std::copy(input, input + rowBytes, output);
    }
    subdesc.setData(std::move(data));
  }

  return subdesc;
}

}
