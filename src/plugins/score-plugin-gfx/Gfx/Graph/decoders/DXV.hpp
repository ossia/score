#pragma once
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

extern "C" {
#include <libavformat/avformat.h>
#include <dxv.h>
}

namespace score::gfx
{
/**
 * @brief Decodes DXV DXT1/DXT5 (Resolume) format to GPU.
 *
 * DXV stores GPU-compressed texture data (BC1/BC3 blocks) with an
 * intermediate compression layer (DXTR opcodes, LZF, or raw).
 * We bypass ffmpeg's CPU decompression and upload raw BC blocks
 * directly to the GPU.
 */
struct DXVDecoder : GPUVideoDecoder
{
  // DXV sub-format tags (little-endian uint32 from packet header)
  enum DXVFormat : uint32_t
  {
    FMT_DXT1 = 0x44585431, // MKBETAG('D','X','T','1')
    FMT_DXT5 = 0x44585435, // MKBETAG('D','X','T','5')
    FMT_YCG6 = 0x59434736, // MKBETAG('Y','C','G','6')
    FMT_YG10 = 0x59473130, // MKBETAG('Y','G','1','0')
  };

  DXVDecoder(QRhiTexture::Format fmt, Video::ImageFormat& d, QString filter = "");

  std::pair<QShader, QShader> init(RenderList& r) override;
  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override;

  // Packet header info
  struct PacketHeader
  {
    DXVFormat format{};
    bool isRaw{};
    bool isOldFormat{};
    const uint8_t* data{};
    int dataSize{};
  };
  static PacketHeader parseHeader(const uint8_t* pkt, int pktSize);

private:
  void uploadTexture(QRhiResourceUpdateBatch& res, const uint8_t* data, std::size_t size);

  QRhiTexture::Format m_format;
  Video::ImageFormat& m_decoder;
  QString m_filter;

  static constexpr int buffer_size = 1024 * 1024 * 16;
  std::unique_ptr<char[]> m_buffer = std::make_unique<char[]>(buffer_size);
};

/**
 * @brief Decodes DXV YCG6/YG10 (YCoCg) format to GPU.
 *
 * YCG6 uses BC4 for Y (full resolution) and BC5 for CoCg (half resolution).
 * YG10 uses BC5 for Y+alpha (full resolution) and BC5 for CoCg (half resolution).
 * The shader converts YCoCg -> RGB.
 */
struct DXVYCoCgDecoder : GPUVideoDecoder
{
  DXVYCoCgDecoder(bool hasAlpha, Video::ImageFormat& d, QString filter = "");
  ~DXVYCoCgDecoder();

  std::pair<QShader, QShader> init(RenderList& r) override;
  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override;

private:
  bool m_hasAlpha; // true for YG10 (has alpha), false for YCG6
  Video::ImageFormat& m_decoder;
  QString m_filter;
  DXVDecompressContext m_ctx;

  static constexpr int buffer_size = 1024 * 1024 * 16;
  std::unique_ptr<char[]> m_yBuffer = std::make_unique<char[]>(buffer_size);
  std::unique_ptr<char[]> m_cocgBuffer = std::make_unique<char[]>(buffer_size);
};

}
