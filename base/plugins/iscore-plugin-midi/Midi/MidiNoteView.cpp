#include "MidiNoteView.hpp"
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QCursor>

namespace Midi
{

NoteView::NoteView(const Note &n, QGraphicsItem *parent):
    QGraphicsItem{parent},
    note{n}
{
    this->setFlag(QGraphicsItem::ItemIsSelectable, true);
    this->setFlag(QGraphicsItem::ItemIsMovable, true);
    this->setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    this->setAcceptHoverEvents(true);
}

void NoteView::paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget)
{
    painter->setRenderHint(QPainter::Antialiasing, false);

    QColor orange = QColor::fromRgb(200, 120, 20);
    painter->setBrush(this->isSelected() ? orange.darker() : orange);
    painter->setPen(orange.darker());
    painter->drawRect(boundingRect().adjusted(0, -1, 0, -1));

    orange.setHslF(orange.hslHueF() - 0.02, 1., 0.25 + (127 - note.velocity()) / 256.);
    QPen p(orange);
    p.setWidthF(2);
    painter->setPen(p);
    painter->drawLine(QPointF{2, m_height / 2.}, QPointF{m_width - 2, m_height/2.});
}

QVariant NoteView::itemChange(
        QGraphicsItem::GraphicsItemChange change,
        const QVariant &value)
{
    switch(change)
    {
        case QGraphicsItem::ItemPositionChange:
        {
            QPointF newPos = value.toPointF();
            QRectF rect = this->parentItem()->boundingRect();
            auto height = rect.height();
            auto note_height = height / 127.;

            newPos.setX(qMin(rect.right(), qMax(newPos.x(), rect.left())));
            // Snap to grid : we round y to the closest multiple of 127
            int note = qBound(
                        0,
                        int(127 - (qMin(rect.bottom(), qMax(newPos.y(), rect.top())) / height) * 127),
                        127);

            newPos.setY(height - note * note_height);
            return newPos;
        }
        case QGraphicsItem::ItemSelectedChange:
        {
            this->setZValue(10 * (int)value.toBool());
            break;
        }
        default:
            break;
    }

    return QGraphicsItem::itemChange(change, value);
}

void NoteView::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    if(event->pos().x() >= this->boundingRect().width() - 2)
    {
        this->setCursor(Qt::SplitHCursor);
    }
    else
    {
        this->setCursor(Qt::ArrowCursor);
    }

    QGraphicsItem::hoverEnterEvent(event);
}

void NoteView::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    if(event->pos().x() >= this->boundingRect().width() - 2)
    {
        this->setCursor(Qt::SplitHCursor);
    }
    else
    {
        this->setCursor(Qt::ArrowCursor);
    }

    QGraphicsItem::hoverMoveEvent(event);

}

void NoteView::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    this->setCursor(Qt::ArrowCursor);

    QGraphicsItem::hoverEnterEvent(event);

}

void NoteView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if(event->pos().x() >= this->boundingRect().width() - 2)
    {
        m_scaling = true;
        event->accept();
    }
    else
    {
        m_scaling = false;
        QGraphicsItem::mousePressEvent(event);
    }
}

void NoteView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if(m_scaling)
    {
        this->setWidth(event->pos().x());
        event->accept();
    }
    else
    {
        QGraphicsItem::mouseMoveEvent(event);
    }
}

void NoteView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if(m_scaling)
    {
        emit noteScaled(m_width / parentItem()->boundingRect().width());
        event->accept();
    }
    else
    {
        emit noteChangeFinished();
        QGraphicsItem::mouseReleaseEvent(event);
    }
}

}
