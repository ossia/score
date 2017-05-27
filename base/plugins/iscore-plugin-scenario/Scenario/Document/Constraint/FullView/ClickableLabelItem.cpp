
#include <QBrush>
#include <QFont>
#include <QPainter>
#include <algorithm>
#include <qnamespace.h>

#include "ClickableLabelItem.hpp"
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/model/Skin.hpp>
#include <Process/Style/ScenarioStyle.hpp>

class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;

namespace Scenario
{

SeparatorItem::SeparatorItem(QGraphicsItem* parent)
    : QGraphicsItem{parent}
{
}

QRectF SeparatorItem::boundingRect() const
{
  return {0., 0., 5., 10.};
}

void SeparatorItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& skin = ScenarioStyle::instance();
  const constexpr QRectF rect{1., 1., 4., 9.};

  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setPen(skin.SeparatorPen);
  painter->setBrush(skin.SeparatorBrush);
  painter->drawLine(rect.bottomLeft(), rect.topRight());
}

ClickableLabelItem::ClickableLabelItem(
    iscore::ModelMetadata& metadata,
    ClickHandler&& onClick,
    const QString& text,
    QGraphicsItem* parent)
    : SimpleTextItem{parent}, m_onClick{std::move(onClick)}
{
  setText(text);
  connect(
      &metadata, &iscore::ModelMetadata::NameChanged, this,
      [&](const QString& name) {
        setText(name);
        emit textChanged();
      });

  this->setFont(ScenarioStyle::instance().Bold10Pt);
  this->setColor(ScenarioStyle::instance().StateOutline);

  this->setAcceptHoverEvents(true);
}

void ClickableLabelItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_onClick(this);
}

void ClickableLabelItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  this->setColor(ScenarioStyle::instance().ConstraintSelected);
}

void ClickableLabelItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  this->setColor(ScenarioStyle::instance().StateOutline);
}

int ClickableLabelItem::index() const
{
  return m_index;
}

void ClickableLabelItem::setIndex(int index)
{
  m_index = index;
}
}
