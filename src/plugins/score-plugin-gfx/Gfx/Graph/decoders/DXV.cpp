
#include <Gfx/Graph/decoders/DXV.hpp>

#include <cstring>

namespace score::gfx
{

// ============================================================================
// Shared: packet header parsing
// ============================================================================

static inline uint32_t readLE32(const uint8_t* p)
{
  return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

DXVDecoder::PacketHeader
DXVDecoder::parseHeader(const uint8_t* pkt, int pktSize)
{
  PacketHeader hdr;
  if(pktSize < 4)
    return hdr;

  uint32_t tag = readLE32(pkt);

  switch(tag)
  {
    case FMT_DXT1:
    case FMT_DXT5:
    case FMT_YCG6:
    case FMT_YG10: {
      if(pktSize < 12)
        return hdr;
      hdr.format = static_cast<DXVFormat>(tag);
      hdr.isRaw = (pkt[6] != 0);
      hdr.data = pkt + 12;
      hdr.dataSize = pktSize - 12;
      hdr.isOldFormat = false;
      return hdr;
    }
    default: {
      uint8_t old_type = tag >> 24;
      hdr.isOldFormat = true;
      hdr.isRaw = (old_type & 0x80) != 0;
      if(old_type & 0x40)
        hdr.format = FMT_DXT5;
      else if((old_type & 0x20) || ((old_type & 0x0F) - 1) == 1)
        hdr.format = FMT_DXT1;
      else
        return hdr;
      hdr.data = pkt + 4;
      hdr.dataSize = pktSize - 4;
      return hdr;
    }
  }
}

// ============================================================================
// Upload helper
// ============================================================================

static void uploadToTexture(
    QRhiResourceUpdateBatch& res, QRhiTexture* tex,
    const uint8_t* data, std::size_t size)
{
  QRhiTextureSubresourceUploadDescription sub;
  sub.setData(QByteArray::fromRawData((const char*)data, size));
  QRhiTextureUploadEntry entry{0, 0, sub};
  QRhiTextureUploadDescription desc{entry};
  res.uploadTexture(tex, desc);
}

// ============================================================================
// DXVDecoder (DXT1/DXT5)
// ============================================================================

// Fragment shader: simple passthrough for BC1/BC3
static inline const QString dxv_fragment = QStringLiteral(R"_(#version 450

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

layout(binding=3) uniform sampler2D y_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

vec4 processTexture(vec4 tex) {
  vec4 processed = tex;
  { %1 }
  return processed;
}

void main ()
{
  fragColor = processTexture(texture(y_tex, v_texcoord));
})_");

DXVDecoder::DXVDecoder(QRhiTexture::Format fmt, Video::ImageFormat& d, QString filter)
    : m_format{fmt}
    , m_decoder{d}
    , m_filter{std::move(filter)}
{
}

std::pair<QShader, QShader> DXVDecoder::init(RenderList& r)
{
  auto& rhi = *r.state.rhi;
  auto shaders
      = score::gfx::makeShaders(r.state, vertexShader(), QString(dxv_fragment).arg(m_filter));

  const auto w = m_decoder.width, h = m_decoder.height;

  {
    auto tex = rhi.newTexture(m_format, QSize{w, h}, 1, QRhiTexture::Flag{});
    tex->create();

    auto sampler = rhi.newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    sampler->create();
    samplers.push_back({sampler, tex});
  }

  return shaders;
}

void DXVDecoder::uploadTexture(
    QRhiResourceUpdateBatch& res, const uint8_t* data, std::size_t size)
{
  uploadToTexture(res, samplers[0].texture, data, size);
}

void DXVDecoder::exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame)
{
  if(!frame.data[0] || frame.linesize[0] <= 0)
    return;

  auto hdr = parseHeader(frame.data[0], frame.linesize[0]);
  if(!hdr.data || hdr.dataSize <= 0)
    return;

  if(hdr.format != FMT_DXT1 && hdr.format != FMT_DXT5)
    return;

  const int bw = (m_decoder.width + 3) / 4;
  const int bh = (m_decoder.height + 3) / 4;
  int texSize = (hdr.format == FMT_DXT1) ? bw * bh * 8 : bw * bh * 16;

  if(texSize > buffer_size)
    return;

  uint8_t* buf = reinterpret_cast<uint8_t*>(m_buffer.get());

  if(hdr.isRaw)
  {
    if(hdr.dataSize < texSize)
      return;
    std::memcpy(buf, hdr.data, texSize);
  }
  else if(hdr.isOldFormat)
  {
    int decompressed = dxv_decompress_lzf(hdr.data, hdr.dataSize, buf, texSize);
    if(decompressed < texSize)
      return;
  }
  else
  {
    int ret;
    if(hdr.format == FMT_DXT1)
      ret = dxv_decompress_dxt1(hdr.data, hdr.dataSize, buf, texSize);
    else
      ret = dxv_decompress_dxt5(hdr.data, hdr.dataSize, buf, texSize);

    if(ret < 0)
      return;
  }

  uploadTexture(res, buf, texSize);
}

// ============================================================================
// DXVYCoCgDecoder (YCG6/YG10)
// ============================================================================

// YCoCg → RGB conversion shader
// Y texture: BC4 (single channel .r) at full resolution
// CoCg texture: BC5 (two channels .rg) at half resolution
// For YG10, alpha comes from BC5 Y texture's second channel
static inline const QString dxv_ycocg_fragment = QStringLiteral(R"_(#version 450

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

layout(binding=3) uniform sampler2D y_tex;
layout(binding=4) uniform sampler2D cocg_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

vec4 processTexture(vec4 tex) {
  vec4 processed = tex;
  { %1 }
  return processed;
}

void main ()
{
  float Y = texture(y_tex, v_texcoord).r;
  vec2 CoCg = texture(cocg_tex, v_texcoord).rg;

  // DXV YCoCg uses offset encoding: Co/Cg are in [0,1], remap to [-0.5, 0.5]
  float Co = CoCg.r - 0.5;
  float Cg = CoCg.g - 0.5;

  float R = Y + Co - Cg;
  float G = Y + Cg;
  float B = Y - Co - Cg;

  fragColor = processTexture(vec4(R, G, B, 1.0));
})_");

// YG10 variant with alpha from Y texture's .g channel (BC5)
static inline const QString dxv_ycocg_alpha_fragment = QStringLiteral(R"_(#version 450

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

layout(binding=3) uniform sampler2D y_tex;
layout(binding=4) uniform sampler2D cocg_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

vec4 processTexture(vec4 tex) {
  vec4 processed = tex;
  { %1 }
  return processed;
}

void main ()
{
  vec2 Ya = texture(y_tex, v_texcoord).rg;
  float Y = Ya.r;
  float alpha = Ya.g;
  vec2 CoCg = texture(cocg_tex, v_texcoord).rg;

  float Co = CoCg.r - 0.5;
  float Cg = CoCg.g - 0.5;

  float R = Y + Co - Cg;
  float G = Y + Cg;
  float B = Y - Co - Cg;

  fragColor = processTexture(vec4(R, G, B, alpha));
})_");

DXVYCoCgDecoder::DXVYCoCgDecoder(bool hasAlpha, Video::ImageFormat& d, QString filter)
    : m_hasAlpha{hasAlpha}
    , m_decoder{d}
    , m_filter{std::move(filter)}
{
  dxv_ctx_init(&m_ctx);
}

DXVYCoCgDecoder::~DXVYCoCgDecoder()
{
  dxv_ctx_free(&m_ctx);
}

std::pair<QShader, QShader> DXVYCoCgDecoder::init(RenderList& r)
{
  auto& rhi = *r.state.rhi;

  const QString& frag = m_hasAlpha ? dxv_ycocg_alpha_fragment : dxv_ycocg_fragment;
  auto shaders
      = score::gfx::makeShaders(r.state, vertexShader(), QString(frag).arg(m_filter));

  const auto w = m_decoder.width, h = m_decoder.height;

  // Y texture: BC4 (YCG6) or BC5 (YG10) at full resolution
  {
    auto fmt = m_hasAlpha ? QRhiTexture::BC5 : QRhiTexture::BC4;
    auto tex = rhi.newTexture(fmt, QSize{w, h}, 1, QRhiTexture::Flag{});
    tex->create();

    auto sampler = rhi.newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    sampler->create();
    samplers.push_back({sampler, tex});
  }

  // CoCg texture: BC5 at half resolution
  {
    auto tex = rhi.newTexture(QRhiTexture::BC5, QSize{w / 2, h / 2}, 1, QRhiTexture::Flag{});
    tex->create();

    auto sampler = rhi.newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    sampler->create();
    samplers.push_back({sampler, tex});
  }

  return shaders;
}

void DXVYCoCgDecoder::exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame)
{
  if(!frame.data[0] || frame.linesize[0] <= 0)
    return;

  DXVDecoder::PacketHeader hdr;
  {
    const uint8_t* pkt = frame.data[0];
    int pktSize = frame.linesize[0];
    if(pktSize < 4) return;
    uint32_t tag = readLE32(pkt);
    if(tag == DXVDecoder::FMT_YCG6 || tag == DXVDecoder::FMT_YG10)
    {
      if(pktSize < 12) return;
      hdr.format = static_cast<DXVDecoder::DXVFormat>(tag);
      hdr.isRaw = (pkt[6] != 0);
      hdr.data = pkt + 12;
      hdr.dataSize = pktSize - 12;
      hdr.isOldFormat = false;
    }
    else
      return;
  }

  if(!hdr.data || hdr.dataSize <= 0)
    return;

  const int w = m_decoder.width;
  const int h = m_decoder.height;
  const int coded_w = (w + 3) & ~3;
  const int coded_h = (h + 3) & ~3;

  // Compute BC texture sizes
  // YCG6: Y = BC4 at full res, CoCg = interleaved BC4 (BC5) at half res
  //   Y tex_size = coded_w / 4 * coded_h / 4 * 8  (BC4: 8 bytes per 4x4 block)
  //   CoCg tex_size = (coded_w/2) / 4 * (coded_h/2) / 4 * 16  (BC5: 16 bytes per 4x4 block)
  // YG10: Y+alpha = BC5 at full res, CoCg = BC5 at half res
  //   Y tex_size = coded_w / 4 * coded_h / 4 * 16
  //   CoCg tex_size = same as YCG6

  int yTexSize, cocgTexSize;
  if(m_hasAlpha)
  {
    // YG10: BC5 = 16 bytes per block for Y+alpha
    yTexSize = (coded_w / 4) * (coded_h / 4) * 16;
  }
  else
  {
    // YCG6: BC4 = 8 bytes per block for Y
    yTexSize = (coded_w / 4) * (coded_h / 4) * 8;
  }
  cocgTexSize = (coded_w / 2 / 4) * (coded_h / 2 / 4) * 16;

  if(yTexSize > buffer_size || cocgTexSize > buffer_size)
    return;

  uint8_t* yBuf = reinterpret_cast<uint8_t*>(m_yBuffer.get());
  uint8_t* cocgBuf = reinterpret_cast<uint8_t*>(m_cocgBuffer.get());

  if(hdr.isRaw)
  {
    // Raw: direct copy (Y data followed by CoCg data)
    if(hdr.dataSize < yTexSize + cocgTexSize)
      return;
    std::memcpy(yBuf, hdr.data, yTexSize);
    std::memcpy(cocgBuf, hdr.data + yTexSize, cocgTexSize);
  }
  else
  {
    int ret;
    if(m_hasAlpha)
      ret = dxv_decompress_yg10(
          &m_ctx, hdr.data, hdr.dataSize,
          yBuf, yTexSize, cocgBuf, cocgTexSize,
          coded_w, coded_h);
    else
      ret = dxv_decompress_ycg6(
          &m_ctx, hdr.data, hdr.dataSize,
          yBuf, yTexSize, cocgBuf, cocgTexSize,
          coded_w, coded_h);

    if(ret < 0)
      return;
  }

  // Upload Y texture
  uploadToTexture(res, samplers[0].texture, yBuf, yTexSize);
  // Upload CoCg texture
  uploadToTexture(res, samplers[1].texture, cocgBuf, cocgTexSize);
}

}
