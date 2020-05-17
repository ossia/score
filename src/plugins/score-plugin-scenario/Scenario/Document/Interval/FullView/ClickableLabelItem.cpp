// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "ClickableLabelItem.hpp"

#include <Process/Style/ScenarioStyle.hpp>

#include <score/model/ModelMetadata.hpp>
#include <score/model/Skin.hpp>

#include <QPainter>
#include <qnamespace.h>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Scenario::ClickableLabelItem)

namespace Scenario
{

SeparatorItem::SeparatorItem(QGraphicsItem* parent) : QGraphicsItem{parent} { }

QRectF SeparatorItem::boundingRect() const
{
  return {0., 0., 5., 10.};
}

void SeparatorItem::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  auto& skin = Process::Style::instance();
  const Q_DECL_RELAXED_CONSTEXPR QRectF rect{1., 1., 4., 9.};

  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setPen(skin.SeparatorPen());
  painter->setBrush(skin.SeparatorBrush());
  painter->drawLine(rect.bottomLeft(), rect.topRight());

  painter->setRenderHint(QPainter::Antialiasing, false);
}

ClickableLabelItem::ClickableLabelItem(
    score::ModelMetadata& metadata,
    ClickHandler&& onClick,
    const QString& text,
    QGraphicsItem* parent)
    : score::SimpleTextItem{score::Skin::instance().Light.main, parent}
    , m_onClick{std::move(onClick)}
{
  setText(text);
  connect(&metadata, &score::ModelMetadata::NameChanged, this, [&](const QString& name) {
    setText(name);
    textChanged();
  });

  this->setFont(score::Skin::instance().Bold10Pt);
  this->setAcceptHoverEvents(true);
}

void ClickableLabelItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_onClick(this);
}

void ClickableLabelItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  this->setColor(score::Skin::instance().Base2.main);
}

void ClickableLabelItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  this->setColor(score::Skin::instance().Light.main);
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
