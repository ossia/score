#pragma once
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/VideoNode.hpp>
#include <Video/VideoInterface.hpp>

namespace score::gfx
{
class GPUVideoDecoder;

/**
 * @brief Whether the GPU decoder has to be rebuilt for this frame.
 *
 * Free and inline rather than a member: a static member of this class is not
 * exported from the plugin, and this has to be callable from a test. A wrong
 * answer here is not a visible defect -- it is a rebuild on every frame, whose
 * only symptom is a frame rate that is quietly worse than it should be.
 *
 * @p built is what the current decoder was built for, @p src what the input
 * reports now, and @p w / @p h the incoming AVFrame's own size -- which for a
 * fielded source is HALF the picture height that @p src carries.
 */
inline bool videoDecoderNeedsRebuild(
    bool hasDecoder, const Video::ImageFormat& built, const Video::ImageFormat& src,
    AVPixelFormat fmt, int w, int h) noexcept
{
  if(!hasDecoder)
    return true;

  // A fielded source hands over half-height frames while describing the whole
  // picture, so the two heights are compared in the same units -- get this
  // wrong and every frame looks like a size change.
  const int pictureH = (src.interlacing == Video::Interlacing::Fields) ? h * 2 : h;

  return fmt != built.pixel_format || w != built.width || pictureH != built.height
         || src.output_format != built.output_format || src.tonemap != built.tonemap
         // The colour description comes from the decoder, not the frame, and a
         // live input may revise it while running -- a sender that starts
         // declaring HDR. The matrix is baked into the shader, so that
         // rebuilds.
         || src.color_space != built.color_space || src.color_range != built.color_range
         || src.color_trc != built.color_trc
         || src.color_primaries != built.color_primaries
         // Interlacing changes the texture geometry; the deinterlace MODE does
         // not -- it is a uniform, so switching weave to bob must not rebuild.
         || src.interlacing != built.interlacing;
}

/**
 * @brief Which mode score_tc should run, as a uniform value.
 *
 * 0 progressive, 1 weave, 2 bob. Weave needs the OTHER half of the stacked
 * texture to hold this field's partner; when it does not -- a dropped field, or
 * the very first field after a connection -- weaving would pair two fields of
 * the same parity and tear. Falling back to bob for that one frame costs half
 * the vertical resolution on that frame and nothing else.
 */
inline float videoFieldMode(
    Video::Interlacing interlacing, Video::Deinterlace deinterlace,
    bool partnerValid) noexcept
{
  if(interlacing != Video::Interlacing::Fields)
    return 0.f;
  if(deinterlace == Video::Deinterlace::Bob || !partnerValid)
    return 2.f;
  return 1.f;
}

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
    // (parity of the newest field, deinterlace mode, unused, unused).
    // std140 puts a vec4 at offset 16, which is where these land. Mirrors
    // material_t in SCORE_GFX_VIDEO_UNIFORMS: change one and change the other,
    // or the shader reads the wrong words and nothing says so.
    float field_parity{}, field_mode{}, field_pad0{}, field_pad1{};
  };

  std::unique_ptr<GPUVideoDecoder> m_gpu;
  std::pair<QShader, QShader> m_shaders;

  Video::ImageFormat m_frameFormat{};

  /// Parity of the most recent field: 0 for field 0 (the even lines), 1 for
  /// field 1. Meaningless unless the format says Fields.
  float m_fieldParity{};

  /// Whether the other half of the stacked texture holds this field's partner.
  /// False after a dropped field, and before the second field of a connection.
  bool m_fieldPartnerValid{};
  bool m_sawField{};
  score::gfx::ScaleMode m_currentScaleMode{};

  std::shared_ptr<RefcountedFrame> m_currentFrame{};
  int64_t m_currentFrameIdx{-1};
  bool m_recomputeScale{};
};

}
