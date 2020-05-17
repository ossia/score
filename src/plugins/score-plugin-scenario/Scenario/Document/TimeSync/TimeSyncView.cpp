// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TimeSyncView.hpp"

#include "TimeSyncPresenter.hpp"

#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/VerticalExtent.hpp>

#include <score/model/ModelMetadata.hpp>

#include <QBrush>
#include <QCursor>
#include <QGraphicsScene>
#include <QPainter>
#include <QPen>
#include <qnamespace.h>

class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
TimeSyncView::TimeSyncView(TimeSyncPresenter& presenter, QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_presenter{presenter}
    , m_color{&presenter.model().metadata().getColor().getBrush()}
    , m_text{m_color->main, this}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setParentItem(parent);
  this->setZValue(ZPos::TimeSync);
  this->setAcceptHoverEvents(true);
  this->setCursor(Qt::CrossCursor);

  m_text.setFont(score::Skin::instance().Bold10Pt);
}

TimeSyncView::~TimeSyncView() { }

void TimeSyncView::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  const auto height = m_extent.bottom() - m_extent.top();
#if !defined(NDEBUG)
  if (m_presenter.model().events().empty())
  {
    QPen ugh(Qt::red, 15);
    QBrush ughb(Qt::red);
    painter->setPen(ugh);
    painter->setBrush(ughb);
    painter->fillRect(QRectF{0, 0, 5, 5}, ughb);
    painter->fillRect(QRectF{0, height, 5, 5}, ughb);
    painter->drawLine(QPointF(0., 0.), QPointF(0., height));
    return;
  }
#endif

  if (height < 1)
    return;

  auto& skin = Process::Style::instance();
  painter->setRenderHint(QPainter::Antialiasing, false);

  if (isSelected())
  {
    painter->setPen(skin.TimenodePen(skin.TimenodeSelected()));
  }
  else
  {
    painter->setPen(skin.TimenodePen(*m_color));
  }

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
  const auto h = m_extent.bottom() - m_extent.top();
  setFlag(ItemHasNoContents, h < 3.);
  this->update();
}

void TimeSyncView::setExtent(VerticalExtent&& extent)
{
  prepareGeometryChange();
  m_extent = std::move(extent);
  const auto h = m_extent.bottom() - m_extent.top();
  setFlag(ItemHasNoContents, h < 3.);
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
  setZValue(m_selected ? ZPos::SelectedTimeSync : ZPos::TimeSync);
  update();
}

void TimeSyncView::changeColor(const score::Brush& newColor)
{
  m_color = &newColor;
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
