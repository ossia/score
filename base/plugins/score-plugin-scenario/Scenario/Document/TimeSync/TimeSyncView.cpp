// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Style/ScenarioStyle.hpp>
#include <QBrush>
#include <QGraphicsScene>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QPen>
#include <algorithm>
#include <qnamespace.h>

#include "TimeSyncPresenter.hpp"
#include "TimeSyncView.hpp"
#include <QCursor>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include <score/model/ModelMetadata.hpp>

class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
TimeSyncView::TimeSyncView(TimeSyncPresenter& presenter, QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_presenter{presenter}
    , m_text{this}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setParentItem(parent);
  this->setZValue(ZPos::TimeSync);
  this->setAcceptHoverEvents(true);
  this->setCursor(Qt::CrossCursor);

  m_color = presenter.model().metadata().getColor();

  m_text.setFont(ScenarioStyle::instance().Bold10Pt);
  m_text.setColor(m_color);
}

TimeSyncView::~TimeSyncView()
{
}

void TimeSyncView::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto height = m_extent.bottom() - m_extent.top();
  if(height < 1)
    return;

  auto& skin = ScenarioStyle::instance();
  painter->setRenderHint(QPainter::Antialiasing, false);

  if (isSelected())
  {
    skin.TimenodePen.setBrush(skin.TimenodeSelected.getBrush());
  }
  else
  {
    skin.TimenodePen.setBrush(m_color.getBrush());
  }

  painter->setPen(skin.TimenodePen);
  painter->drawLine(QPointF(0., 0.), QPointF(0., height));

#if defined(SCORE_SCENARIO_DEBUG_RECTS)
  painter->setPen(Qt::darkMagenta);
  painter->drawRect(boundingRect());
#endif
}

void TimeSyncView::setExtent(const VerticalExtent& extent)
{
  prepareGeometryChange();
  m_extent = extent;
  this->update();
}

void TimeSyncView::setExtent(VerticalExtent&& extent)
{
  prepareGeometryChange();
  m_extent = std::move(extent);
  this->update();
}

void TimeSyncView::setMoving(bool arg)
{
  update();
}

void TimeSyncView::setTriggerActive(bool b)
{
  if (b)
    m_text.setPos(-m_text.boundingRect().width() / 2, -40);
  else
    m_text.setPos(-m_text.boundingRect().width() / 2, -20);
}

void TimeSyncView::setSelected(bool selected)
{
  m_selected = selected;
  update();
}

void TimeSyncView::changeColor(score::ColorRef newColor)
{
  m_color = newColor;
  this->update();
}

void TimeSyncView::setLabel(const QString& s)
{
  m_text.setText(s);

  // Used to re-set the text position
  setTriggerActive(m_text.pos().y() == -40);
}

void TimeSyncView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (event->button() == Qt::MouseButton::LeftButton)
    m_presenter.pressed(event->scenePos());
}

void TimeSyncView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  m_presenter.moved(event->scenePos());
}

void TimeSyncView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  m_presenter.released(event->scenePos());
}
}
