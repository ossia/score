
#include <Gfx/Graph/decoders/HAP.hpp>

namespace score::gfx
{
HAPDecoder::HAPSection HAPDecoder::HAPSection::read(const uint8_t* bytes)
{
  HAPSection s;

  s.type = bytes[3];

  if(bytes[0] == 0 && bytes[1] == 0 && bytes[2] == 0)
  {
    // bytes 4,5,6,7 hold the size
    s.size += bytes[7] << 24;
    s.size += bytes[6] << 16;
    s.size += bytes[5] << 8;
    s.size += bytes[4];
    s.data = bytes + 8;
  }
  else
  {
    // bytes 0, 1, 2 hold the size
    s.size += bytes[2] << 16;
    s.size += bytes[1] << 8;
    s.size += bytes[0];
    s.data = bytes + 4;
  }

  return s;
}
void HAPDecoder::exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame)
{
  auto section = HAPSection::read(frame.data[0]);
  if(section.type == 0x0D)
    return;

  switch(section.type >> 4)
  {
    case 0xA:
      setPixels_noEncoding(res, section.data, section.size);
      break;
    case 0xB:
      setPixels_snappy(res, section.data, section.size);
      break;
    case 0xC: {

      HapDecodeCallback cb
          = [](HapDecodeWorkFunction function, void* p, unsigned int count, void* info) {
        for(std::size_t i = 0; i < count; i++)
          function(p, i);
      };
      void* ctx = nullptr;
      void* output = m_buffer.get();
      unsigned long outBytes = buffer_size;
      unsigned long outBytesUsed{};
      unsigned int outFormat{};
      auto r = HapDecode(
          frame.data[0], frame.linesize[0], 0, cb, ctx, output, outBytes, &outBytesUsed,
          &outFormat);
      if(r == HapResult_No_Error)
        setPixels_noEncoding(res, (const uint8_t*)output, outBytesUsed);
      else
        qDebug() << r;
      break;
    }
  }
}

void HAPDecoder::setPixels_noEncoding(
    QRhiResourceUpdateBatch& res, const uint8_t* data_start, std::size_t size)
{
  QRhiTextureSubresourceUploadDescription sub;
  sub.setData(QByteArray::fromRawData((const char*)data_start, size));
  QRhiTextureUploadEntry entry{0, 0, sub};

  QRhiTextureUploadDescription desc{entry};

  auto y_tex = samplers[0].texture;
  res.uploadTexture(y_tex, desc);
}

void HAPDecoder::setPixels_snappy(
    QRhiResourceUpdateBatch& res, const uint8_t* data_start, std::size_t size)
{
  size_t uncomp_size{};
  snappy::GetUncompressedLength((const char*)data_start, size, &uncomp_size);

  QByteArray data(uncomp_size, Qt::Uninitialized);
  snappy::RawUncompress((const char*)data_start, size, data.data());

  QRhiTextureSubresourceUploadDescription sub;
  sub.setData(std::move(data));
  QRhiTextureUploadEntry entry{0, 0, sub};

  QRhiTextureUploadDescription desc{entry};

  auto y_tex = samplers[0].texture;
  res.uploadTexture(y_tex, desc);
}

HAPDefaultDecoder::HAPDefaultDecoder(
    QRhiTexture::Format fmt, Video::ImageFormat& d, QString f)
    : format{fmt}
    , decoder{d}
    , filter{f}
{
}

std::pair<QShader, QShader> HAPDefaultDecoder::init(RenderList& r)
{
  auto& rhi = *r.state.rhi;
  auto shaders
      = score::gfx::makeShaders(r.state, vertexShader(), QString(fragment).arg(filter));

  const auto w = decoder.width, h = decoder.height;

  {
    auto tex = rhi.newTexture(format, QSize{w, h}, 1, QRhiTexture::Flag{});
    tex->create();

    auto sampler = rhi.newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    sampler->create();
    samplers.push_back({sampler, tex});
  }

  return shaders;
}

HAPMDecoder::HAPMDecoder(Video::ImageFormat& d, QString f)
    : decoder{d}
    , filter{f}
{
}

std::pair<QShader, QShader> HAPMDecoder::init(RenderList& r)
{
  auto& rhi = *r.state.rhi;
  auto shaders
      = score::gfx::makeShaders(r.state, vertexShader(), QString(fragment).arg(filter));

  const auto w = decoder.width, h = decoder.height;

  // Color texture
  {
    auto tex = rhi.newTexture(QRhiTexture::BC3, QSize{w, h}, 1, QRhiTexture::Flag{});
    tex->create();

    auto sampler = rhi.newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    sampler->create();
    samplers.push_back({sampler, tex});
  }
  // Alpha texture
  {
    auto tex = rhi.newTexture(QRhiTexture::BC4, QSize{w, h}, 1, QRhiTexture::Flag{});
    tex->create();

    auto sampler = rhi.newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    sampler->create();
    samplers.push_back({sampler, tex});
  }

  return shaders;
}

void HAPMDecoder::exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame)
{
  HapDecodeCallback cb
      = [](HapDecodeWorkFunction function, void* p, unsigned int count, void* info) {
    for(std::size_t i = 0; i < count; i++)
      function(p, i);
  };
  void* ctx = nullptr;
  void* ycocg_output = m_buffer.get();
  unsigned long ycocg_outBytes = buffer_size;
  unsigned long ycocg_outBytesUsed{};
  unsigned int ycocg_outFormat{};
  auto r = HapDecode(
      frame.data[0], frame.linesize[0], 0, cb, ctx, ycocg_output, ycocg_outBytes,
      &ycocg_outBytesUsed, &ycocg_outFormat);
  if(r == HapResult_No_Error)
  {
    void* alpha_output = m_alphaBuffer.get();
    unsigned long alpha_outBytes = buffer_size;
    unsigned long alpha_outBytesUsed{};
    unsigned int alpha_outFormat{};
    r = HapDecode(
        frame.data[0], frame.linesize[0], 1, cb, ctx, alpha_output, alpha_outBytes,
        &alpha_outBytesUsed, &alpha_outFormat);
    if(r == HapResult_No_Error)
    {
      setPixels(
          res, samplers[0].texture, (const uint8_t*)ycocg_output, ycocg_outBytesUsed);
      setPixels(
          res, samplers[1].texture, (const uint8_t*)alpha_output, alpha_outBytesUsed);
    }
  }
  else
  {
    qDebug() << r;
  }
}

void HAPMDecoder::setPixels(
    QRhiResourceUpdateBatch& res, QRhiTexture* tex, const uint8_t* ycocg_start,
    std::size_t ycocg_size)
{
  QRhiTextureSubresourceUploadDescription sub;
  sub.setData(QByteArray::fromRawData((const char*)ycocg_start, ycocg_size));
  QRhiTextureUploadEntry entry{0, 0, sub};

  QRhiTextureUploadDescription desc{entry};

  res.uploadTexture(tex, desc);
}

}
