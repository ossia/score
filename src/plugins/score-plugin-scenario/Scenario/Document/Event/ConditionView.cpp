// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ConditionView.hpp"

#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/Event/EventModel.hpp>

#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPoint>
#include <QSize>
#include <QVector>
#include <qnamespace.h>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::ConditionView)
namespace Scenario
{
static constexpr const qreal ConditionCHeight = 27.;
static const QPainterPath conditionTrianglePath{[] {
  QPainterPath p;
  QPainterPathStroker s;
  s.setCapStyle(Qt::RoundCap);
  s.setJoinStyle(Qt::RoundJoin);
  s.setWidth(2);

  p.addPolygon(QVector<QPointF>{QPointF(25, 5), QPointF(25, 21), QPointF(32, 14)});
  p.closeSubpath();

  return (p + s.createStroke(p)).simplified();
}()};

ConditionView::ConditionView(const EventModel& model, QGraphicsItem* parent)
    : QGraphicsItem{parent}, m_model{model}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  setFlag(ItemStacksBehindParent, true);
  setCursor(Qt::CursorShape::CrossCursor);
  setHeight(0.);
}

QRectF ConditionView::boundingRect() const
{
  constexpr qreal m_width = 40.;
  constexpr double penWidth = 0.;
  return QRectF{-penWidth, -penWidth, m_width + penWidth, m_height + ConditionCHeight + penWidth};
}

void ConditionView::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  auto& style = Process::Style::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);

  const score::Brush& col
      = !m_selected ? ExecutionStatusProperty{m_model.status()}.conditionStatusColor(style)
                    : style.IntervalSelected();

  painter->setPen(style.ConditionPen(col));
  painter->setBrush(style.NoBrush());
  painter->drawPath(m_Cpath);

#if !defined(SCORE_IEEE_SKIN)
  painter->setPen(style.ConditionTrianglePen(col));
  painter->setBrush(col);

  painter->fillPath(conditionTrianglePath, col);
#endif

  painter->setRenderHint(QPainter::Antialiasing, false);
}

void ConditionView::changeHeight(qreal newH)
{
  if (qFuzzyCompare(m_height, newH))
    return;
  setHeight(newH);
}

void ConditionView::setSelected(bool selected)
{
  m_selected = selected;
  update();
}

void ConditionView::setHeight(qreal newH)
{
  prepareGeometryChange();
  m_height = newH;

  m_Cpath = QPainterPath();
  static constexpr const QSizeF conditionCSize{ConditionCHeight, ConditionCHeight};

  const auto brect = boundingRect();
  QRectF rect(brect.topLeft(), conditionCSize);
  QRectF bottomRect(
      QPointF(brect.bottomLeft().x(), brect.bottomLeft().y() - ConditionCHeight), conditionCSize);

  m_Cpath.moveTo(brect.width() / 2., 2.);
  m_Cpath.arcTo(rect, 60., 120.);
  m_Cpath.lineTo(0., m_height + ConditionCHeight / 2.);
  m_Cpath.arcTo(bottomRect, -180., 120.);

  QPainterPathStroker stk;
  stk.setWidth(1.);
  m_strokedCpath = stk.createStroke(m_Cpath);

  this->update();
}

void ConditionView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (event->button() == Qt::MouseButton::LeftButton && contains(event->pos()))
  {
    event->accept();
    pressed(event->scenePos());
  }
  else
  {
    event->ignore();
  }
}
}

QPainterPath Scenario::ConditionView::shape() const
{
  return m_strokedCpath;
}

bool Scenario::ConditionView::contains(const QPointF& point) const
{
  return m_Cpath.contains(point) || m_strokedCpath.contains(point)
         || conditionTrianglePath.contains(point);
}

QPainterPath Scenario::ConditionView::opaqueArea() const
{
  return m_strokedCpath;
}
