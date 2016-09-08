#include "MidiView.hpp"
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

namespace Midi
{

View::View(QGraphicsItem *parent):
    Process::LayerView {parent}
{
    this->setFlag(QGraphicsItem::ItemIsFocusable, true);

}

View::~View()
{

}

void View::paint_impl(QPainter* p) const
{
    //    1 3   6 8 10
    //   0 2 4 5 7 9  11
    QColor l1 = QColor::fromRgb(200, 200, 200);
    QColor d1 = QColor::fromRgb(170, 170, 170);
    QColor d2 = d1.darker();
    p->setPen(d2);
    auto rect = boundingRect();
    auto note_height = rect.height() / 127.;

    for(int i = 0; i < 128; i++)
    {
        switch(i%12)
        {
            case 0:
            case 2:
            case 4:
            case 5:
            case 7:
            case 9:
            case 11:
            {
                p->setBrush(l1);
                break;
            }

            case 1:
            case 3:
            case 6:
            case 8:
            case 10:
            {
                p->setBrush(d1);
                break;
            }
        }

        p->drawRect(0, rect.height() - note_height * i,rect.width(), note_height );
    }
}

void View::contextMenuEvent(
        QGraphicsSceneContextMenuEvent* event)
{
    emit askContextMenu(event->screenPos(), event->scenePos());
    event->accept();
}

void View::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
    emit pressed();
    ev->accept();
}

void View::mouseDoubleClickEvent(QGraphicsSceneMouseEvent * ev)
{
    emit doubleClicked(ev->pos());
    ev->accept();
}

NoteData noteAtPos(QPointF point, const QRectF& rect)
{
    NoteData n;
    n.start = qBound(0., point.x() / rect.width(), 1.);
    n.duration = 0.1;
    n.pitch = qBound(0, int(127 - (qBound(rect.bottom(), point.y(), rect.top()) / rect.height()) * 127), 127);
    n.velocity = 127.;
    return n;
}

}
