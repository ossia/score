#pragma once
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/VideoNode.hpp>
#include <Video/VideoInterface.hpp>
namespace Video
{
class VideoDecoder;
}
namespace score::gfx
{
class GPUVideoDecoder;

class VideoNodeRendererBase : public NodeRenderer
{
public:
  explicit VideoNodeRendererBase(const VideoNodeBase& node) noexcept;
  ~VideoNodeRendererBase();

  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& edge) override;
  TextureRenderTarget renderTargetForInput(const Port& input) override;

  void release(RenderList& r) override;
  void createPipelines(RenderList& r);
  void createGpuDecoder();
  void setupGpuDecoder(RenderList& r);
  void checkFormat(RenderList& r, AVPixelFormat fmt, int w, int h);
  void displayFrame(AVFrame& frame, RenderList& renderer, QRhiResourceUpdateBatch& res);
  void postprocess(RenderList& renderer, QRhiResourceUpdateBatch& res);

  const VideoNodeBase& node;
  Video::ImageFormat m_frameFormat{};

  PassMap m_p;
  QRhiBuffer* m_meshBuffer{};
  QRhiBuffer* m_idxBuffer{};
  QRhiBuffer* m_processUBO{};
  QRhiBuffer* m_materialUBO{};

  struct Material
  {
    float scale_w{}, scale_h{};
  };

  std::unique_ptr<GPUVideoDecoder> m_gpu;
  score::gfx::ScaleMode m_currentScaleMode{};
  int64_t m_currentFrameIdx{-1};
  bool m_recomputeScale{};
};

class BasicVideoNodeRenderer : public VideoNodeRendererBase
{
public:
  explicit BasicVideoNodeRenderer(
      const VideoNodeBase& node,
      const std::shared_ptr<::Video::VideoDecoder>& frames) noexcept;
  ~BasicVideoNodeRenderer();

  BasicVideoNodeRenderer() = delete;
  BasicVideoNodeRenderer(const BasicVideoNodeRenderer&) = delete;
  BasicVideoNodeRenderer(BasicVideoNodeRenderer&&) = delete;
  BasicVideoNodeRenderer& operator=(const BasicVideoNodeRenderer&) = delete;
  BasicVideoNodeRenderer& operator=(BasicVideoNodeRenderer&&) = delete;

  void update(RenderList& renderer, QRhiResourceUpdateBatch& res) override;

private:
  std::shared_ptr<::Video::VideoDecoder> m_video;
  AVFrame* m_curFrame{};
  int m_prevFrame = -1;
};

class VideoNodeRenderer : public VideoNodeRendererBase
{
public:
  explicit VideoNodeRenderer(
      const VideoNodeBase& node, VideoFrameShare& frames) noexcept;
  ~VideoNodeRenderer();

  VideoNodeRenderer() = delete;
  VideoNodeRenderer(const VideoNodeRenderer&) = delete;
  VideoNodeRenderer(VideoNodeRenderer&&) = delete;
  VideoNodeRenderer& operator=(const VideoNodeRenderer&) = delete;
  VideoNodeRenderer& operator=(VideoNodeRenderer&&) = delete;

  void update(RenderList& renderer, QRhiResourceUpdateBatch& res) override;

private:
  Video::VideoMetadata& decoder() const noexcept;

  VideoFrameShare& reader;
  std::shared_ptr<RefcountedFrame> m_currentFrame{};
};

}
