#pragma once
#include <Process/LayerView.hpp>
#include <Process/ZoomHelper.hpp>

#include <ossia/detail/flat_map.hpp>

namespace Video
{
class VideoThumbnailer;
}
namespace Gfx::Video
{
class Model;
class View final : public Process::LayerView
{
public:
  explicit View(const Model&, QGraphicsItem* parent);
  ~View() override;

  void setZoom(ZoomRatio r);

private:
  void onPathChanged(const QString& str);
  void widthChanged(qreal) override;

  void dropEvent(QGraphicsSceneDragDropEvent* event) override;
  void paint_impl(QPainter*) const override;

  ::Video::VideoThumbnailer* m_thumb{};
  ossia::flat_map<int64_t, QImage> m_images;
  double m_zoom{1.};
  int64_t m_lastRequestIndex{};
};
}
