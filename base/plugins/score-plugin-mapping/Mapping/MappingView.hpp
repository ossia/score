#pragma once

#include <Process/LayerView.hpp>
#include <Curve/CurveView.hpp>
#include <QString>
#include <QTextLayout>

class QGraphicsItem;
class QPainter;

namespace Mapping
{
class LayerView final : public Process::LayerView
{
    Q_OBJECT
public:
  explicit LayerView(QGraphicsItem* parent);
  QPixmap pixmap() override;
  void setCurveView(Curve::View* view){ m_curveView = view; };

private:
  void paint_impl(QPainter* painter) const override;
  Curve::View* m_curveView{};
};
}
