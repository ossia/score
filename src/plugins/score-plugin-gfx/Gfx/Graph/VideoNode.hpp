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

  virtual ~VideoNode();

  score::gfx::NodeRenderer*
  createRenderer(RenderList& r) const noexcept override;

  void seeked();

  void setScaleMode(score::gfx::ScaleMode s);
private:
  friend VideoNodeRenderer;
  std::shared_ptr<Video::VideoInterface> m_decoder;
  QString m_filter;
  std::optional<double> m_nativeTempo;
  score::gfx::ScaleMode m_scaleMode{};
};

}
