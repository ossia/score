#include <Process/Style/ScenarioStyle.hpp>
#include <QBrush>
#include <QGraphicsScene>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QPen>
#include <algorithm>
#include <qnamespace.h>

#include "TimeNodePresenter.hpp"
#include "TimeNodeView.hpp"
#include <QCursor>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include <iscore/model/ModelMetadata.hpp>


class QWidget;

namespace Scenario
{
TimeNodeView::TimeNodeView(TimeNodePresenter& presenter, QQuickPaintedItem* parent)
    : GraphicsItem{parent}, m_presenter{presenter}
{
  //this->setCacheMode(QQuickPaintedItem::NoCache);
  this->setParentItem(parent);
  this->setZ(ZPos::TimeNode);
  this->setAcceptHoverEvents(true);
  this->setCursor(Qt::CrossCursor);

  auto& skin = iscore::Skin::instance();

  m_text = new SimpleTextItem{this};
  m_color = presenter.model().metadata().getColor();

  auto f = skin.SansFont;
  f.setPointSize(10);
  m_text->setFont(f);
  m_text->setColor(m_color);

  setWidth(6);
}

TimeNodeView::~TimeNodeView()
{
}

void TimeNodeView::paint(
    QPainter* painter)
{
  auto height = m_extent.bottom() - m_extent.top();
  if(height < 1)
    return;

  auto& skin = ScenarioStyle::instance();
  painter->setRenderHint(QPainter::Antialiasing, false);
  QBrush pen_color;
  if (isSelected())
  {
    pen_color = skin.TimenodeSelected.getColor();
  }
  else
  {
    pen_color = m_color.getColor();
  }

  skin.TimenodePen.setBrush(pen_color);
  painter->setPen(skin.TimenodePen);

  painter->drawLine(QPointF(0, 0), QPointF(0, height));

#if defined(ISCORE_SCENARIO_DEBUG_RECTS)
  painter->setPen(Qt::darkMagenta);
  painter->drawRect(boundingRect());
#endif
}

void TimeNodeView::setExtent(const VerticalExtent& extent)
{
  m_extent = extent;
  setHeight(m_extent.bottom() - m_extent.top());
  this->update();
}

void TimeNodeView::setExtent(VerticalExtent&& extent)
{
  m_extent = std::move(extent);
  setHeight(m_extent.bottom() - m_extent.top());
  this->update();
}

void TimeNodeView::setMoving(bool arg)
{
  update();
}

void TimeNodeView::setTriggerActive(bool b)
{
  if (b)
    m_text->setPosition(QPointF(-m_text->boundingRect().width() / 2, -40));
  else
    m_text->setPosition(QPointF(-m_text->boundingRect().width() / 2, -20));
}

void TimeNodeView::setSelected(bool selected)
{
  m_selected = selected;
  update();
}

void TimeNodeView::changeColor(iscore::ColorRef newColor)
{
  m_color = newColor;
  this->update();
}

void TimeNodeView::setLabel(const QString& s)
{
  m_text->setText(s);

  // Used to re-set the text position
  setTriggerActive(m_text->y() == -40);
}

void TimeNodeView::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::MouseButton::LeftButton)
    emit m_presenter.pressed(mapToScene(event->localPos()));
}

void TimeNodeView::mouseMoveEvent(QMouseEvent* event)
{
  emit m_presenter.moved(mapToScene(event->localPos()));
}

void TimeNodeView::mouseReleaseEvent(QMouseEvent* event)
{
  emit m_presenter.released(mapToScene(event->localPos()));
}
}
