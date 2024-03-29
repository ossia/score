// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "IntervalHeader.hpp"

#include "IntervalPresenter.hpp"
#include "IntervalView.hpp"

#include <QCursor>

class QGraphicsSceneMouseEvent;
namespace Scenario
{
void IntervalHeader::setWidth(double width)
{
  prepareGeometryChange();
  m_width = width;
  auto& skin = score::Skin::instance();
  this->setCursor(skin.CursorOpenHand);
  update();
}

void IntervalHeader::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  auto& skin = score::Skin::instance();
  this->setCursor(skin.CursorClosedHand);
  m_view->mousePressEvent(event);
  m_view->presenter().pressed(event->scenePos());
  event->accept();
}

void IntervalHeader::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  m_view->mouseMoveEvent(event);
  event->accept();
}

void IntervalHeader::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  auto& skin = score::Skin::instance();
  this->setCursor(skin.CursorOpenHand);
  m_view->mouseReleaseEvent(event);
  event->accept();
}
}
