
#include <Process/Style/ScenarioStyle.hpp>
#include <QBrush>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QList>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <algorithm>
#include <cmath>

#include "TemporalConstraintHeader.hpp"
#include <Scenario/Document/Constraint/ConstraintHeader.hpp>
#include <iscore/model/Skin.hpp>

class QGraphicsSceneMouseEvent;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
TemporalConstraintHeader::TemporalConstraintHeader() : ConstraintHeader{}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setAcceptDrops(true);
  this->setAcceptedMouseButtons(
      Qt::LeftButton); // needs to be enabled for dblclick
  this->setFlags(
      QGraphicsItem::ItemIsSelectable |
      QGraphicsItem::ItemClipsToShape |
      QGraphicsItem::ItemClipsChildrenToShape);

}

QRectF TemporalConstraintHeader::boundingRect() const
{
  return {0., 0., m_width, qreal(ConstraintHeader::headerHeight())};
}

void TemporalConstraintHeader::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const auto& skin = ScenarioStyle::instance();
  painter->setRenderHint(QPainter::Antialiasing, false);
  if (m_state == State::RackHidden)
  {
    const auto rect = boundingRect();
    painter->fillRect(
        rect, skin.ConstraintHeaderRackHidden.getColor());

    // Fake timenode continuation
    painter->setPen(skin.ConstraintHeaderSeparator);
    painter->drawLine(rect.topLeft(), rect.bottomLeft());
    painter->drawLine(rect.topRight(), rect.bottomRight());
    painter->drawLine(rect.bottomLeft(), rect.bottomRight());
  }
  else
  {
    painter->setPen(skin.ConstraintHeaderSeparator);
    painter->drawLine(
                QPointF{0., (double)ConstraintHeaderHeight},
                QPointF{m_width, (double)ConstraintHeaderHeight});
  }

  // Header
  painter->setPen(skin.ConstraintHeaderText.getColor().color());

  // If the centered text is hidden, we put it at the left so that it's on the
  // view.
  // We have to compute the visible part of the header
  auto view = scene()->views().first();
  int text_left
      = view->mapFromScene(
                mapToScene({m_width / 2. - m_textWidthCache / 2., 0.}))
            .x();
  int text_right
      = view->mapFromScene(
                mapToScene({m_width / 2. + m_textWidthCache / 2., 0.}))
            .x();
  double x = (m_width - m_textWidthCache) / 2.;
  const constexpr double min_x = 10.;
  const double max_x = view->width() - 30.;

  if (text_left <= min_x)
  {
    // Compute the pixels needed to add to have top-left at 0
    x = x - text_left + min_x;
  }
  else if (text_right >= max_x)
  {
    // Compute the pixels needed to add to have top-right at max
    x = x - text_right + max_x;
  }

  x = std::max(x, 10.);
  if (std::abs(m_previous_x - x) > 0.1)
  {
    m_previous_x = x;
  }
  // TODO do like TimeRuler
  painter->setFont(skin.Bold12Pt);
  painter->drawText(QRectF{m_previous_x, 0, m_width - x, ConstraintHeader::headerHeight()},
                    Qt::AlignLeft | Qt::AlignVCenter, m_text);
}

void TemporalConstraintHeader::mouseDoubleClickEvent(
    QGraphicsSceneMouseEvent* event)
{
  emit doubleClicked();
}

void TemporalConstraintHeader::on_textChange()
{
  QFontMetrics fm(ScenarioStyle::instance().Bold12Pt);
  m_textWidthCache = fm.width(m_text);
}

void TemporalConstraintHeader::hoverEnterEvent(QGraphicsSceneHoverEvent* h)
{
  QGraphicsItem::hoverEnterEvent(h);
  emit constraintHoverEnter();
}

void TemporalConstraintHeader::hoverLeaveEvent(QGraphicsSceneHoverEvent* h)
{
  QGraphicsItem::hoverLeaveEvent(h);
  emit constraintHoverLeave();
}

void TemporalConstraintHeader::dragEnterEvent(
    QGraphicsSceneDragDropEvent* event)
{
  QGraphicsItem::dragEnterEvent(event);
  event->accept();
}

void TemporalConstraintHeader::dragLeaveEvent(
    QGraphicsSceneDragDropEvent* event)
{
  QGraphicsItem::dragLeaveEvent(event);
  event->accept();
}

void TemporalConstraintHeader::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  emit dropReceived(event->pos(), event->mimeData());

  event->accept();
}
}
