// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Style/ScenarioStyle.hpp>
#include <QColor>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <QPolygon>
#include <QSize>
#include <QVector>
#include <qnamespace.h>

#include "ConditionView.hpp"

class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
ConditionView::ConditionView(score::ColorRef color, QGraphicsItem* parent)
    : QGraphicsItem{parent}, m_color{color}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  setFlag(ItemStacksBehindParent, true);

  changeHeight(0);
}

QRectF ConditionView::boundingRect() const
{
  return QRectF{0, 0, m_width, m_height + m_CHeight};
}

void ConditionView::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& style = ScenarioStyle::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);
  const QBrush& col = m_color.getBrush();
  style.ConditionPen.setBrush(col);
  style.ConditionTrianglePen.setBrush(col);

  painter->setPen(style.ConditionPen);
  painter->setBrush(style.TransparentBrush);
  painter->drawPath(m_Cpath);

#if !defined(SCORE_IEEE_SKIN)
  painter->setPen(style.ConditionTrianglePen);
  painter->setBrush(col);

  static const QPainterPath trianglePath{
      [] {
          QPainterPath p;
          QPainterPathStroker s;
          s.setCapStyle(Qt::RoundCap);
          s.setJoinStyle(Qt::RoundJoin);
          s.setWidth(2);

          p.addPolygon(QVector<QPointF>{
                           QPointF(25, 5),
                           QPointF(25, 21),
                           QPointF(32, 14)});
          p.closeSubpath();

          return p + s.createStroke(p);
      }()
  };
  painter->fillPath(trianglePath, col);
#endif

  painter->setRenderHint(QPainter::Antialiasing, false);
}

void ConditionView::changeHeight(qreal newH)
{
  prepareGeometryChange();
  m_height = newH;

  m_Cpath = QPainterPath();

  QRectF rect(boundingRect().topLeft(), QSize(m_CHeight, m_CHeight));
  QRectF bottomRect(
      QPointF(
          boundingRect().bottomLeft().x(),
          boundingRect().bottomLeft().y() - m_CHeight),
      QSize(m_CHeight, m_CHeight));

  m_Cpath.moveTo(boundingRect().width() / 2., 2);
  m_Cpath.arcTo(rect, 60, 120);
  m_Cpath.lineTo(0, m_height + m_CHeight / 2);
  m_Cpath.arcTo(bottomRect, -180, 120);
  this->update();
}

void ConditionView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::MouseButton::LeftButton)
      emit pressed(event->scenePos());
}
}
