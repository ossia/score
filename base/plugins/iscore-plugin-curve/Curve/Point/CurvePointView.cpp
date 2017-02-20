#include <Curve/CurveStyle.hpp>
#include <QColor>
#include <QCursor>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QPen>
#include <QtGlobal>
#include <iscore/selection/Selectable.hpp>
#include <qnamespace.h>

#include "CurvePointModel.hpp"
#include "CurvePointView.hpp"
#include <iscore/tools/Todo.hpp>


class QWidget;
#include <iscore/model/Identifier.hpp>
namespace Curve
{
const qreal radius = 2.5;
const QRectF ellipse{-radius, -radius, 2 * radius, 2 * radius};

PointView::PointView(
    const PointModel* model, const Curve::Style& style, QQuickPaintedItem* parent)
    : QQuickPaintedItem{parent}, m_style{style}
{
  this->setZ(2);
  this->setCursor(Qt::CrossCursor);
  this->setFlag(ItemIsFocusable, false);
  // Bad on retina. :( this->setCacheMode(QQuickPaintedItem::DeviceCoordinateCache);

  setModel(model);
}

void PointView::setModel(const PointModel* model)
{
  m_model = model;
  if (m_model)
  {
    con(m_model->selection, &Selectable::changed, this,
        &PointView::setSelected);
  }
}

const PointModel& PointView::model() const
{
  return *m_model;
}

const Id<PointModel>& PointView::id() const
{
  return m_model->id();
}

QRectF PointView::boundingRect() const
{
  const qreal gripSize = radius * 2;
  return {-gripSize, -gripSize, 2 * gripSize, 2 * gripSize};
}

void PointView::paint(
    QPainter* painter)
{
  painter->setRenderHint(QPainter::Antialiasing, true);
  if(!m_selected)
  {
    painter->setPen(m_style.PenPoint);
    painter->setBrush(m_style.BrushPoint);
  }
  else
  {
    painter->setPen(m_style.PenPointSelected);
    painter->setBrush(m_style.BrushPointSelected);
  }

  painter->drawEllipse(ellipse);
  painter->setRenderHint(QPainter::Antialiasing, false);
}

void PointView::setSelected(bool selected)
{
  m_selected = selected;
  update();
}

void PointView::enable()
{
  this->setVisible(true);
}

void PointView::disable()
{
  this->setVisible(false);
}
/*
void PointView::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
  emit contextMenuRequested(ev->screenPos(), ev->scenePos());
}*/
}
