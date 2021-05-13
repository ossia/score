#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <score_lib_base_export.h>

#include <verdigris>

namespace score
{

struct RightClickImpl;
template <typename T>
struct QGraphicsSliderBase : public QGraphicsItem
{
  QGraphicsSliderBase(QGraphicsItem* parent);
  ~QGraphicsSliderBase();

  bool isInHandle(QPointF p);
  double getHandleX() const;
  QRectF sliderRect() const;
  QRectF handleRect() const;

  void setRect(const QRectF& r);
  QRectF boundingRect() const override;

  QRectF m_rect{defaultSliderSize};
  RightClickImpl* impl{};
};

}
