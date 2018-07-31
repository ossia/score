#pragma once

#include <Curve/CurveView.hpp>
#include <wobjectdefs.h>
#include <Process/LayerView.hpp>
#include <QString>
#include <QTextLayout>

class QGraphicsItem;
class QPainter;

namespace Mapping
{
class LayerView final : public Process::LayerView
{
  W_OBJECT(LayerView)
public:
  explicit LayerView(QGraphicsItem* parent);
  QPixmap pixmap() noexcept override;
  void setCurveView(Curve::View* view)
  {
    m_curveView = view;
  }

private:
  void paint_impl(QPainter* painter) const override;
  Curve::View* m_curveView{};
};
}
