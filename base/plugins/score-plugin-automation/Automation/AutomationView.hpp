#pragma once

#include <Process/LayerView.hpp>
#include <Curve/CurveView.hpp>
#include <QString>
#include <QTextLayout>

class QGraphicsItem;
class QPainter;
class QMimeData;

namespace Automation
{
class LayerView final : public Process::LayerView
{
  Q_OBJECT
public:
  explicit LayerView(QGraphicsItem* parent);
  virtual ~LayerView();

  QPixmap pixmap() override;
  void setCurveView(Curve::View* view){ m_curveView = view; };

signals:
  void dropReceived(const QMimeData& mime);

protected:
  void paint_impl(QPainter* painter) const override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  Curve::View* m_curveView{};
};
}
