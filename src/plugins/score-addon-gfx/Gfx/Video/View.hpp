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

  void paint_impl(QPainter*) const override;

  const Model& m_model;
  ::Video::VideoThumbnailer* m_thumb{};
  ossia::flat_map<int64_t, QImage> m_images;
  double m_zoom{1.};
};
}
