// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "IntervalBrace.hpp"

#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalView.hpp>

#include <QCursor>
#include <QPainter>

namespace Scenario
{
LeftBraceView::~LeftBraceView() {}

RightBraceView::~RightBraceView() {}
IntervalBrace::IntervalBrace(
    const IntervalView& parentCstr,
    QGraphicsItem* parent)
    : QGraphicsItem(), m_parent{parentCstr}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  auto& skin = score::Skin::instance();
  this->setCursor(skin.CursorScaleH);
  this->setZValue(ZPos::Brace);

  m_path.moveTo(10, -10);
  m_path.arcTo(0, -10, 20, 20, 90, 180);

  this->setParentItem(parent);
}

QRectF IntervalBrace::boundingRect() const
{
  return {0, -10, 10, 20};
}

void IntervalBrace::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  auto& skin = Process::Style::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setBrush(skin.NoBrush());

  if (m_parent.isSelected())
  {
    painter->setPen(skin.IntervalBraceSelected());
  }
  else if (m_parent.warning())
  {
    painter->setPen(skin.IntervalBraceWarning());
  }
  else if (!m_parent.isValid())
  {
    painter->setPen(skin.IntervalBraceInvalid());
  }
  else
  {
    painter->setPen(skin.IntervalBrace());
  }

  painter->drawPath(m_path);

#if defined(SCORE_SCENARIO_DEBUG_RECTS)
  painter->setPen(Qt::lightGray);
  painter->setBrush(Qt::NoBrush);
  painter->drawRect(boundingRect());
#endif

  painter->setRenderHint(QPainter::Antialiasing, false);
}

void IntervalBrace::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (event->button() == Qt::MouseButton::LeftButton)
    m_parent.presenter().pressed(event->scenePos());
}

void IntervalBrace::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  m_parent.presenter().moved(event->scenePos());
}

void IntervalBrace::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  m_parent.presenter().released(event->scenePos());
}
}
