#pragma once
#include <Process/LayerView.hpp>
#include <Curve/CurveView.hpp>
#include <QTextLayout>

namespace Interpolation
{
class View final : public Process::LayerView
{
  Q_OBJECT
public:
  explicit View(QGraphicsItem* parent);
  virtual ~View();

  QPixmap pixmap() override;
  void setCurveView(Curve::View* view){ m_curveView = view; };

signals:
  void dropReceived(const QMimeData& mime);

protected:
  void paint_impl(QPainter* painter) const override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

private:
  Curve::View* m_curveView;
};
}
