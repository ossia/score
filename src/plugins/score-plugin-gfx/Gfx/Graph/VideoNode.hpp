#pragma once

#include <Gfx/Graph/Node.hpp>
namespace Video
{
struct VideoInterface;
}
namespace score::gfx
{
class VideoNodeRenderer;
/**
 * @brief Model for rendering a video
 */
struct SCORE_PLUGIN_GFX_EXPORT VideoNode : ProcessNode
{
public:
  VideoNode(
      std::shared_ptr<Video::VideoInterface> dec,
      std::optional<double> nativeTempo,
      QString f = {});

  const Mesh& mesh() const noexcept override;
  virtual ~VideoNode();

  score::gfx::NodeRenderer*
  createRenderer(RenderList& r) const noexcept override;

  void seeked();

private:
  friend VideoNodeRenderer;
  std::shared_ptr<Video::VideoInterface> m_decoder;
  std::optional<double> m_nativeTempo;
  QString m_filter;

  const TexturedTriangle& m_mesh = TexturedTriangle::instance();
};

}
