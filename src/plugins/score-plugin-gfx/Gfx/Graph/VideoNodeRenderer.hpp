#pragma once
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/VideoNode.hpp>
#include <Video/VideoInterface.hpp>

namespace score::gfx
{
class GPUVideoDecoder;

class VideoNodeRenderer : public NodeRenderer
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

  TextureRenderTarget renderTargetForInput(const Port& input) override;

  void createGpuDecoder();
  void setupGpuDecoder(RenderList& r);
  void checkFormat(RenderList& r, AVPixelFormat fmt, int w, int h);

  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& edge) override;

  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override;
  void release(RenderList& r) override;

  void initState(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void releaseState(RenderList& renderer) override;
  void addOutputPass(
      RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res) override;
  void removeOutputPass(RenderList& renderer, Edge& edge) override;
  bool hasOutputPassForEdge(Edge& edge) const override;

private:
  void createPipelines(RenderList& r);
  void displayFrame(AVFrame& frame, RenderList& renderer, QRhiResourceUpdateBatch& res);
  Video::VideoMetadata& decoder() const noexcept;

  const VideoNodeBase& node() const noexcept
  {
    return static_cast<const VideoNodeBase&>(NodeRenderer::node);
  }
  VideoFrameShare& reader;

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
  std::pair<QShader, QShader> m_shaders;

  Video::ImageFormat m_frameFormat{};
  score::gfx::ScaleMode m_currentScaleMode{};

  std::shared_ptr<RefcountedFrame> m_currentFrame{};
  int64_t m_currentFrameIdx{-1};
  bool m_recomputeScale{};
};

}
