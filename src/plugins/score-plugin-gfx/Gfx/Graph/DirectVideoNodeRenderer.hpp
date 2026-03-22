#pragma once
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/VideoNode.hpp>
#include <Video/VideoInterface.hpp>

#include <vector>

class QRhi;

extern "C" {
struct AVFormatContext;
struct AVCodecContext;
struct AVStream;
struct AVFrame;
struct AVPacket;
struct AVBufferRef;
struct SwsContext;
#include <libavutil/pixfmt.h>
}

namespace Video
{
class Rescale;
}

namespace score::gfx
{
class GPUVideoDecoder;
struct PixelFormatInfo;

/**
 * @brief Renderer for intra-only video codecs with instant seeking.
 *
 * Unlike VideoNodeRenderer which reads from a frame queue fed by a background
 * thread, this renderer owns its own LibAV decoding context and performs
 * synchronous seek + decode + GPU upload directly in update().
 *
 * This enables instant seeking for all-intra codecs (ProRes, MJPEG, DNxHD, HAP, etc.)
 * where every frame is independently decodable.
 *
 * Supports hardware-accelerated decoding when available (VAAPI, D3D11VA, CUDA,
 * VideoToolbox, Vulkan Video). When zero-copy GPU texture import is not possible
 * for the current RHI backend, falls back to av_hwframe_transfer_data() which
 * still avoids software decode CPU cost.
 */
class DirectVideoNodeRenderer : public NodeRenderer
{
public:
  explicit DirectVideoNodeRenderer(
      const VideoNodeBase& node, const Video::VideoMetadata& metadata) noexcept;
  ~DirectVideoNodeRenderer();

  DirectVideoNodeRenderer() = delete;
  DirectVideoNodeRenderer(const DirectVideoNodeRenderer&) = delete;
  DirectVideoNodeRenderer(DirectVideoNodeRenderer&&) = delete;
  DirectVideoNodeRenderer& operator=(const DirectVideoNodeRenderer&) = delete;
  DirectVideoNodeRenderer& operator=(DirectVideoNodeRenderer&&) = delete;

  TextureRenderTarget renderTargetForInput(const Port& input) override;

  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& edge) override;
  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override;
  void release(RenderList& r) override;

private:
  const VideoNodeBase& node() const noexcept
  {
    return static_cast<const VideoNodeBase&>(NodeRenderer::node);
  }

  bool openFile(score::gfx::GraphicsApi api, QRhi* rhi = nullptr);
  void closeFile();
  bool seekAndDecode(int64_t flicks);
  bool isSequentialRead(int64_t flicks) const;
  bool readNextPacketRaw();
  bool readNextPacketAVCodec();

  void createGpuDecoder(QRhi& rhi);
  score::gfx::PixelFormatInfo hwPixelFormatInfo() const;
  std::unique_ptr<GPUVideoDecoder> tryCreateZeroCopyDecoder(QRhi& rhi);
  void setupGpuDecoder(RenderList& r);
  void createPipelines(RenderList& r);

  // Hardware decoding
  AVPixelFormat selectHardwareAcceleration(
      score::gfx::GraphicsApi api, int codec_id) const;
  bool setupHardwareDecoder(
      const void* codec, AVPixelFormat hwPixFmt);
  static enum AVPixelFormat negotiateHWFormat(
      AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts);
  void transferHWFrame();

  // Video file info
  std::string m_filePath;
  Video::ImageFormat m_frameFormat{};
  double m_fps{};
  double m_flicks_per_dts{};
  double m_dts_per_flicks{};
  bool m_useAVCodec{true};

  // Own LibAV context
  AVFormatContext* m_formatContext{};
  AVCodecContext* m_codecContext{};
  const void* m_codec{}; // AVCodec*
  AVStream* m_avstream{};
  AVFrame* m_decodedFrame{};

  // Hardware decode state
  AVBufferRef* m_hwDeviceCtx{};
  AVPixelFormat m_hwPixelFormat{AV_PIX_FMT_NONE};
  AVPixelFormat m_hwSwFormat{AV_PIX_FMT_NONE};
  std::vector<const char*> m_vkEnabledExtensions; // kept alive for FFmpeg
  AVFrame* m_swTransferFrame{};
  score::gfx::GraphicsApi m_rhiApi{};
  QRhi* m_rhi{};
  bool m_hwSwFormatChecked{false};
  bool m_zeroCopyFailed{false};
  bool m_sharedVulkanDevice{false}; // true if FFmpeg uses QRhi's VkDevice

  // Render state
  PassMap m_p;
  MeshBuffers m_meshBuffer{};
  QRhiBuffer* m_processUBO{};
  QRhiBuffer* m_materialUBO{};

  struct Material
  {
    float scale_w{}, scale_h{};
    float tex_w{}, tex_h{};
  };

  std::unique_ptr<GPUVideoDecoder> m_gpu;
  score::gfx::ScaleMode m_currentScaleMode{};

  int64_t m_lastRequestedFlicks{-1};
  int64_t m_lastDecodedDts{INT64_MIN};
  bool m_recomputeScale{true};
};

}
