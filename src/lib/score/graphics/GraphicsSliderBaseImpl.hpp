#pragma once
#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/graphics/GraphicWidgets.hpp>
#include <score/widgets/DoubleSpinBox.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QGraphicsProxyWidget>

namespace score
{
template <typename T>
QGraphicsSliderBase<T>::QGraphicsSliderBase(QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , impl{new RightClickImpl}
{
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

template <typename T>
QGraphicsSliderBase<T>::~QGraphicsSliderBase()
{
  if (this->impl->spinbox || this->impl->spinboxProxy)
    delete this->impl->spinboxProxy;
  delete impl;
}

template <typename T>
QRectF QGraphicsSliderBase<T>::boundingRect() const
{
  return m_rect;
}

template <typename T>
bool QGraphicsSliderBase<T>::isInHandle(QPointF p)
{
  return m_rect.contains(p);
}

template <typename T>
double QGraphicsSliderBase<T>::getHandleX() const
{
  return sliderRect().width() * static_cast<const T&>(*this).m_value;
}

template <typename T>
double QGraphicsSliderBase<T>::getExecHandleX() const
{
  return sliderRect().width() * static_cast<const T&>(*this).m_execValue;
}

template <typename T>
QRectF QGraphicsSliderBase<T>::sliderRect() const
{
  return QRectF{0, 0, m_rect.width(), 8};
}

template <typename T>
QRectF QGraphicsSliderBase<T>::handleRect() const
{
  auto r = sliderRect();
  r.setWidth(std::max(0., static_cast<const T&>(*this).getHandleX()));
  return r;
}
template <typename T>
QRectF QGraphicsSliderBase<T>::execHandleRect() const
{
  return {0,  6, static_cast<const T&>(*this).getExecHandleX(), 2};
}

template <typename T>
void QGraphicsSliderBase<T>::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
}

}
