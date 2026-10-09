#pragma once
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/VideoNode.hpp>
#include <Gfx/Graph/VideoNodeRenderer.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
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
#include <libavutil/rational.h>
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
class SCORE_PLUGIN_GFX_EXPORT DirectVideoNodeRenderer : public NodeRenderer
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
  void runInitialPasses(
      RenderList&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res,
      Edge& edge) override;
  QRhiTexture* textureForOutput(const Port& output) override;
  void release(RenderList& r) override;

  void initState(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void releaseState(RenderList& renderer) override;
  void addOutputPass(
      RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res) override;
  void removeOutputPass(RenderList& renderer, Edge& edge) override;
  bool hasOutputPassForEdge(Edge& edge) const override;

private:
  const VideoNodeBase& node() const noexcept
  {
    return static_cast<const VideoNodeBase&>(NodeRenderer::node);
  }

  bool openFile(score::gfx::GraphicsApi api, QRhi* rhi = nullptr);
  void closeFile();
  bool seekAndDecode(int64_t flicks);
  //! Whether the frame on screen is the one for this playback time, as far as
  //! is known without decoding: update() decodes only when it is not.
  bool holdsTime(int64_t flicks) const noexcept;
  //! Seeks to the keyframe at or before `pts`, in the stream's time base.
  bool seekTo(int64_t pts);
  //! Playback time of a timestamp, and back: time 0 is the stream's start.
  int64_t ptsToFlicks(int64_t pts) const noexcept;
  int64_t flicksToPts(int64_t flicks) const noexcept;
  bool isSequentialRead(int64_t flicks) const;
  bool readVideoPacket(AVPacket* into);
  bool readNextPacketRaw();
  bool peekNextPacketRaw();
  //! The next frame, or packet for the GPU-direct codecs, and read-ahead.
  bool readNext();
  bool peekNext();
  //! The next frame in display order: the one read ahead, else a new one.
  bool readNextPacketAVCodec();
  //! Decodes the next frame into `into`, with its display timestamp and
  //! duration.
  bool decodeNextFrame(AVFrame* into, int64_t& ts, int64_t& duration);
  //! Reads the next frame ahead and keeps it for readNextPacketAVCodec.
  bool peekNextFrame();
  void dropPeekedFrame() noexcept;

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
  AVRational m_timeBase{0, 1};
  //! The timestamp shown at playback time 0.
  int64_t m_startPts{};
  //! One frame in the stream's time base, for frames that do not carry a
  //! duration.
  int64_t m_framePts{1};
  //! The demuxer declares no timestamps (raw elementary streams): frames are
  //! numbered, and seeks restart from the last known keyframe.
  bool m_noTimestamps{};
  int64_t m_nextMissingPts{};
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

  using Material = VideoMaterialUBO;

  std::unique_ptr<GPUVideoDecoder> m_gpu;
  QShader m_cachedVertexShader;
  QShader m_cachedFragmentShader;
  score::gfx::ScaleMode m_currentScaleMode{};
  QSize m_scaleRenderSize{};

  int64_t m_lastRequestedFlicks{-1};
  int64_t m_lastDecodedDts{INT64_MIN};
  int64_t m_lastDecodedDuration{};
  AVFrame* m_peekedFrame{};
  int64_t m_peekedDts{};
  int64_t m_peekedDuration{};
  bool m_hasPeekedFrame{};
  AVPacket* m_peekedPacket{};
  bool m_hasPeekedPacket{};
  //! The largest pts - dts seen: how far the demuxer's seek key is ahead.
  int64_t m_ptsLead{};
  //! Without timestamps: the keyframes read so far, as (frame, byte position).
  std::vector<std::pair<int64_t, int64_t>> m_keyframes;
  int64_t m_packetNumber{};
  bool m_recomputeScale{true};
  VideoOwnTexture m_ownTexture;
};

}
