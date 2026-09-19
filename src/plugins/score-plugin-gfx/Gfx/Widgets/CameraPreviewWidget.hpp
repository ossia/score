#pragma once
#include <QWidget>

#include <score_plugin_gfx_export.h>

#include <memory>

namespace Video
{
class ExternalInput;
struct VideoMetadata;
}

namespace score::gfx
{
struct Graph;
class CameraNode;
}

namespace Gfx
{
class RhiPreviewWidget;

/**
 * @brief Live preview of a Video::ExternalInput, for device settings dialogs.
 *
 * Renders through the ordinary score::gfx path -- CameraNode into a private
 * Graph, read back by RhiPreviewWidget -- so what the dialog shows is what
 * the device will produce, decoders and colour handling included.
 */
class SCORE_PLUGIN_GFX_EXPORT CameraPreviewWidget : public QWidget
{
public:
  explicit CameraPreviewWidget(QWidget* parent = nullptr);
  ~CameraPreviewWidget() override;

  //! Takes ownership of the input's lifetime: starts it, stops it on clear().
  void setInput(std::shared_ptr<Video::ExternalInput> input);
  void clear();

  //! Fields are only meaningful once the input has delivered a frame.
  const Video::VideoMetadata* metadata() const noexcept;

private:
  void timerEvent(QTimerEvent* ev) override;
  void attach();
  void detach();

  std::unique_ptr<score::gfx::Graph> m_graph;
  std::unique_ptr<score::gfx::CameraNode> m_node;
  std::shared_ptr<Video::ExternalInput> m_input;
  RhiPreviewWidget* m_rhi{};
  int m_timerId{};
};
}
