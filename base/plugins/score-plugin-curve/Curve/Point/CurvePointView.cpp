// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Curve/CurveStyle.hpp>
#include <QColor>
#include <QCursor>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QPen>
#include <QtGlobal>
#include <score/selection/Selectable.hpp>
#include <qnamespace.h>

#include "CurvePointModel.hpp"
#include "CurvePointView.hpp"
#include <score/tools/Todo.hpp>

class QStyleOptionGraphicsItem;
class QWidget;
#include <score/model/Identifier.hpp>
namespace Curve
{
static const qreal radius = 2.85;
static const QRectF ellipse{-radius, -radius, 2. * radius, 2. * radius};
static const QPolygonF ellipsePath{[] {
    QPainterPath p;
    p.addEllipse(ellipse);
    return p.simplified().toFillPolygon();
}()};
PointView::PointView(
    const PointModel* model, const Curve::Style& style, QGraphicsItem* parent)
    : QGraphicsItem{parent}, m_style{style}
{
  this->setZValue(2);
  this->setCursor(Qt::CrossCursor);
  this->setFlag(ItemIsFocusable, false);
  // Bad on retina. :( this->setCacheMode(QGraphicsItem::DeviceCoordinateCache);

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
  const qreal gripSize = radius * 2.;
  return {-gripSize, -gripSize, 2. * gripSize, 2. * gripSize};
}

void PointView::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
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

  painter->drawPolygon(ellipsePath);
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

void PointView::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
  contextMenuRequested(ev->screenPos(), ev->scenePos());
}
}
