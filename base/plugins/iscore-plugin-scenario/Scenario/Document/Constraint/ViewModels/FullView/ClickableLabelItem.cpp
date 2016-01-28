#include <Process/Style/ProcessFonts.hpp>
#include <QFont>
#include <qnamespace.h>
#include <algorithm>
#include <QBrush>

#include "ClickableLabelItem.hpp"
#include <Process/ModelMetadata.hpp>

class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;

namespace Scenario
{

SeparatorItem::SeparatorItem(QGraphicsItem *parent):
    QGraphicsSimpleTextItem{"/", parent}
{
    auto font = Process::Fonts::Sans();
    font.setPointSize(10);
    font.setBold(true);
    this->setFont(font);
    this->setBrush(Qt::white);
}


ClickableLabelItem::ClickableLabelItem(
        ModelMetadata& metadata,
        ClickHandler&& onClick,
        const QString &text,
        QGraphicsItem *parent):
    QGraphicsSimpleTextItem{text, parent},
    m_onClick{std::move(onClick)}
{
    connect(&metadata, &ModelMetadata::nameChanged,
            this, [&] (const QString& name) {
        setText(name);
        emit textChanged();
    });

    auto font = Process::Fonts::Sans();
    font.setPointSize(10);
    font.setBold(true);
    this->setFont(font);
    this->setBrush(Qt::white);

    this->setAcceptHoverEvents(true);
}


void ClickableLabelItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_onClick(this);
}


void ClickableLabelItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    this->setBrush(Qt::blue);
    update();
}

void ClickableLabelItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    this->setBrush(Qt::white);
    update();
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
