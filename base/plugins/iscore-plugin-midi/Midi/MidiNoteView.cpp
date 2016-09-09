#include "MidiNoteView.hpp"

namespace Midi
{

NoteView::NoteView(const Note &n, QGraphicsItem *parent):
    QGraphicsItem{parent},
    note{n}
{
    this->setFlag(QGraphicsItem::ItemIsSelectable, true);
    this->setFlag(QGraphicsItem::ItemIsMovable, true);
    this->setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
}

void NoteView::paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget)
{
    const QColor orange = QColor::fromRgb(200, 120, 20);
    painter->setBrush(this->isSelected() ? orange.darker() : orange);
    painter->setPen(orange.darker());
    painter->drawRect(boundingRect());
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
            emit noteChanged(note, newPos.x() / rect.width());
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

void NoteView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    emit noteChangeFinished();
    QGraphicsItem::mouseReleaseEvent(event);
}

}
