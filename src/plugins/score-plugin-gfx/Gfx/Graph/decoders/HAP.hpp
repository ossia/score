#pragma once
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <snappy.h>
#include <hap/source/hap.h>
struct HAPDecoder : GPUVideoDecoder
{
  static const constexpr auto rgb_filter = R"_(#version 450
    layout(std140, binding = 0) uniform buf {
    mat4 clipSpaceCorrMatrix;
    vec2 texcoordAdjust;
    } tbuf;

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
      vec2 texcoord = vec2(v_texcoord.x, tbuf.texcoordAdjust.y + tbuf.texcoordAdjust.x * v_texcoord.y);

      fragColor = processTexture(texture(y_tex, texcoord));
    })_";


  HAPDecoder(QRhiTexture::Format fmt, NodeModel& n, video_decoder& d, QString f = "")
    : format{fmt}
    , node{n}
    , decoder{d}
    , filter{f}
  { }
  QRhiTexture::Format format;
  NodeModel& node;
  video_decoder& decoder;
  QString filter;

  struct HAPSection
  {
    static HAPSection read(const uint8_t* bytes)
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

    uint32_t type{};
    uint32_t size{};
    const uint8_t* data{};
  };

  void init(Renderer& r, RenderedNode& rendered) override
  {
    auto& rhi = *r.state.rhi;
    std::tie(node.m_vertexS, node.m_fragmentS) = makeShaders(node.mesh().defaultVertexShader(), QString(rgb_filter).arg(filter));

    const auto w = decoder.width, h = decoder.height;

    {
      auto tex = rhi.newTexture(format, QSize{w, h}, 1, QRhiTexture::Flag{});
      tex->build();

      auto sampler = rhi.newSampler(
          QRhiSampler::Linear,
          QRhiSampler::Linear,
          QRhiSampler::None,
          QRhiSampler::ClampToEdge,
          QRhiSampler::ClampToEdge);
      sampler->build();
      rendered.m_samplers.push_back({sampler, tex});
    }
  }

  void exec(Renderer&, RenderedNode& rendered, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    auto section = HAPSection::read(frame.data[0]);
    if(section.type == 0x0D)
      return;

    switch (section.type >> 4)
    {
      case 0xA:
        setPixels_noEncoding(rendered, res, section.data, section.size);
        break;
      case 0xB:
        setPixels_snappy(rendered, res, section.data, section.size);
        break;
      case 0xC:
      {

        HapDecodeCallback cb = [] (HapDecodeWorkFunction function, void *p, unsigned int count, void *info) {
          for(int i = 0; i < count; i++)
            function(p, i);
        };
        void* ctx = nullptr;
        void* output = m_buffer.get();
        unsigned long outBytes = buffer_size;
        unsigned long outBytesUsed{};
        unsigned int outFormat{};
        auto r = HapDecode(frame.data[0], frame.linesize[0], 0, cb, ctx, output, outBytes, &outBytesUsed, &outFormat);
        if(r == HapResult_No_Error)
          setPixels_noEncoding(rendered, res, (const uint8_t*)output, outBytesUsed);
        else
          qDebug() << r;
        break;
      }
    }
  }

  void setPixels_noEncoding(RenderedNode& rendered, QRhiResourceUpdateBatch& res, const uint8_t* data_start, std::size_t size)
  {
    QRhiTextureSubresourceUploadDescription sub;
    sub.setData(QByteArray::fromRawData((const char*)data_start, size));
    QRhiTextureUploadEntry entry{0, 0, sub};

    QRhiTextureUploadDescription desc{entry};

    auto y_tex = rendered.m_samplers[0].texture;
    res.uploadTexture(y_tex, desc);
  }

  void setPixels_snappy(RenderedNode& rendered, QRhiResourceUpdateBatch& res, const uint8_t* data_start, std::size_t size)
  {
    size_t uncomp_size{};
    snappy::GetUncompressedLength((const char*)data_start, size, &uncomp_size);

    QByteArray data(uncomp_size, Qt::Uninitialized);
    snappy::RawUncompress((const char*)data_start, size, data.data());

    QRhiTextureSubresourceUploadDescription sub;
    sub.setData(std::move(data));
    QRhiTextureUploadEntry entry{0, 0, sub};

    QRhiTextureUploadDescription desc{entry};

    auto y_tex = rendered.m_samplers[0].texture;
    res.uploadTexture(y_tex, desc);
  }

  void setPixels_manual(RenderedNode& rendered, QRhiResourceUpdateBatch& res, const uint8_t* data_start, std::size_t size)
  {
    auto section = HAPSection::read(data_start);

    if(section.type != 0x01)
      return;

    struct
    {
      HAPSection compTable;
      HAPSection sizeTables;
      std::optional<HAPSection> ofstTable;
    } decodeContainer;

    qDebug() << "zection zeirezerrzerez" << section.size;
    for(auto* data = section.data; data < section.data + section.size; data++)
    {
      fprintf(stderr, "%02X ", *data);
    }
    std::cerr << std::endl;
    return;
    auto end = section.data + section.size;
    std::vector<HAPSection> decodeContainers;
    auto nextSection = HAPSection::read(section.data);
    while(nextSection.data < end)
    {
      decodeContainers.push_back(nextSection);
      nextSection = HAPSection::read(section.data + 4);
    }

    qDebug() << "num sections:" << decodeContainers.size();
    for(auto cont : decodeContainers)
      qDebug() << cont.type;
    /*
    qDebug() << " decomp size: " << section.size;

    decodeContainer.compTable = HAPSection::read(section.data + section.size);
    qDebug() << " compTable : " << decodeContainer.compTable.type<< decodeContainer.compTable.size;
    decodeContainer.sizeTable = HAPSection::read(decodeContainer.compTable.data + decodeContainer.compTable.size);
    HAPSection rem = HAPSection::read(decodeContainer.sizeTable.data + decodeContainer.sizeTable.size);
    if(rem.type == 0x04);
    decodeContainer.ofstTable = rem;
*/

  }

  void release(Renderer&, RenderedNode& n) override
  {
    for (auto [sampler, tex] : n.m_samplers)
      tex->releaseAndDestroyLater();
  }

  static constexpr int buffer_size = 1024 * 1024 * 16;
  std::unique_ptr<char[]> m_buffer = std::make_unique<char[]>(1024 * 1024 * 16);
};


struct HAPYCoCgDecoder : GPUVideoDecoder
{
  static const constexpr auto rgb_filter = R"_(#version 450
    layout(std140, binding = 0) uniform buf {
    mat4 clipSpaceCorrMatrix;
    vec2 texcoordAdjust;
    } tbuf;

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
      vec2 texcoord = vec2(v_texcoord.x, tbuf.texcoordAdjust.y + tbuf.texcoordAdjust.x * v_texcoord.y);

      fragColor = processTexture(texture(y_tex, texcoord));
    })_";


  HAPYCoCgDecoder(QRhiTexture::Format fmt, NodeModel& n, video_decoder& d, QString f = "")
    : format{fmt}
    , node{n}
    , decoder{d}
    , filter{f}
  { }
  QRhiTexture::Format format;
  NodeModel& node;
  video_decoder& decoder;
  QString filter;

  void init(Renderer& r, RenderedNode& rendered) override
  {
    auto& rhi = *r.state.rhi;
    std::tie(node.m_vertexS, node.m_fragmentS) = makeShaders(node.mesh().defaultVertexShader(), QString(rgb_filter).arg(filter));

    const auto w = decoder.width, h = decoder.height;

    {
      auto tex = rhi.newTexture(format, QSize{w, h}, 1, QRhiTexture::Flag{});
      tex->build();

      auto sampler = rhi.newSampler(
          QRhiSampler::Linear,
          QRhiSampler::Linear,
          QRhiSampler::None,
          QRhiSampler::ClampToEdge,
          QRhiSampler::ClampToEdge);
      sampler->build();
      rendered.m_samplers.push_back({sampler, tex});
    }
  }

  void exec(Renderer&, RenderedNode& rendered, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    auto bytes = frame.data[0];
    uint32_t size{};
    uint8_t type = bytes[3];
    uint8_t* data_start{};
    if(bytes[0] == 0 && bytes[1] == 0 && bytes[2] == 0)
    {
      // bytes 4,5,6,7 hold the size
      size += bytes[7] << 24;
      size += bytes[6] << 16;
      size += bytes[5] << 8;
      size += bytes[4];
      data_start = bytes + 8;
    }
    else
    {
      // bytes 0, 1, 2 hold the size
      size += bytes[2] << 16;
      size += bytes[1] << 8;
      size += bytes[0];
      data_start = bytes + 4;
    }

    if(type == 0x0D)
      return;

    enum TextureCompression { BC1, BC3, YCoCg_BC3, BC4, BC7 } textureCompression{};
    enum Coding { None, Snappy, Manual } coding{};

    switch (type >> 4)
    {
      case 0xA:
        coding = None;
        break;
      case 0xB:
        coding = Snappy;
        break;
      case 0xC:
        coding = Manual;
        break;
    }
    switch (type & 0x0F)
    {
      case 0x1:
        textureCompression = BC4;
        break;
      case 0xB:
        textureCompression = BC1;
        break;
      case 0xC:
        textureCompression = BC7;
        break;
      case 0xE:
        textureCompression = BC3;
        break;
      case 0xF:
        textureCompression = YCoCg_BC3;
        break;
    }

    switch(coding)
    {
      case None:
      {
        QRhiTextureSubresourceUploadDescription sub;
        sub.setData(QByteArray::fromRawData((const char*)data_start, size));
        QRhiTextureUploadEntry entry{0, 0, sub};

        QRhiTextureUploadDescription desc{entry};

        auto y_tex = rendered.m_samplers[0].texture;
        res.uploadTexture(y_tex, desc);
        break;
      }
      case Snappy:
      {
        size_t uncomp_size{};
        snappy::GetUncompressedLength((const char*)data_start, size, &uncomp_size);

        QByteArray data(uncomp_size, Qt::Uninitialized);
        snappy::RawUncompress((const char*)data_start, size, data.data());

        QRhiTextureSubresourceUploadDescription sub;
        sub.setData(std::move(data));
        QRhiTextureUploadEntry entry{0, 0, sub};

        QRhiTextureUploadDescription desc{entry};

        auto y_tex = rendered.m_samplers[0].texture;
        res.uploadTexture(y_tex, desc);
        break;
      }
      case Manual:
      {
        break;
      }
    }
  }

  void release(Renderer&, RenderedNode& n) override
  {
    for (auto [sampler, tex] : n.m_samplers)
      tex->releaseAndDestroyLater();
  }
};

