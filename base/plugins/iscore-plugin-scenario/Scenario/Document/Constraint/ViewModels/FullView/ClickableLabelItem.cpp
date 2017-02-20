#include <Process/Style/ProcessFonts.hpp>
#include <QBrush>
#include <QFont>
#include <algorithm>
#include <qnamespace.h>

#include "ClickableLabelItem.hpp"
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/model/Skin.hpp>
#include <QPainter>

class QHoverEvent;
class QGraphicsSceneMouseEvent;

namespace Scenario
{

SeparatorItem::SeparatorItem(QQuickPaintedItem* parent)
    : QQuickPaintedItem{parent}
{
  /*
  auto font = iscore::Skin::instance().SansFont;
  font.setPointSize(10);
  font.setBold(true);
  this->setFont(font);
  this->setBrush(Qt::white);
  */
}

void SeparatorItem::paint(QPainter* painter)
{
  painter->drawText(boundingRect(), "/");
}

ClickableLabelItem::ClickableLabelItem(
    iscore::ModelMetadata& metadata,
    ClickHandler&& onClick,
    const QString& text,
    QQuickPaintedItem* parent)
    : QQuickPaintedItem{parent}, m_onClick{std::move(onClick)}, m_text{text}
{
  connect(
      &metadata, &iscore::ModelMetadata::NameChanged, this,
      [&](const QString& name) {
        m_text = name;
        update();
        emit textChanged();
      });
/*
  auto font = iscore::Skin::instance().SansFont;
  font.setPointSize(10);
  font.setBold(true);
  this->setFont(font);
  this->setBrush(Qt::white);
*/
  this->setAcceptHoverEvents(true);
}

void ClickableLabelItem::mousePressEvent(QMouseEvent* event)
{
  m_onClick(this);
}

void ClickableLabelItem::hoverEnterEvent(QHoverEvent* event)
{
//  this->setBrush(Qt::blue);
  update();
}

void ClickableLabelItem::hoverLeaveEvent(QHoverEvent* event)
{
//  this->setBrush(Qt::white);
  update();
}

void ClickableLabelItem::paint(QPainter* painter)
{
  painter->drawText(boundingRect(), "TODO");
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
