#include "CommentBlockView.hpp"

#include <QPainter>
#include <QWidget>
#include <QFont>
#include <QGraphicsSceneMouseEvent>

#include <Scenario/Document/CommentBlock/CommentBlockPresenter.hpp>

CommentBlockView::CommentBlockView(
        CommentBlockPresenter& presenter,
        QGraphicsObject* parent):
    QGraphicsObject{parent},
    m_presenter{presenter}
{
    this->setParentItem(parent);
    this->setZValue(1);
    this->setAcceptHoverEvents(true);

    m_textItem = new QGraphicsTextItem{"Hello dear user !", this};
    m_textItem->setTextInteractionFlags(Qt::TextEditorInteraction);
    m_textItem->setDefaultTextColor(Qt::white);
}

void CommentBlockView::paint(QPainter* painter,
                             const QStyleOptionGraphicsItem* option,
                             QWidget* widget)
{
    auto p = QPen{Qt::white};
    p.setWidth(1.);
    painter->setPen(p);
    painter->drawRoundedRect(boundingRect(), 5, 5);
}

QRectF CommentBlockView::boundingRect() const
{
    if(m_textItem)
    {
        auto rect = m_textItem->boundingRect();
        rect.translate(-3, -3);
        rect.setWidth(rect.width() + 6);
        rect.setHeight(rect.height() + 6);
        return rect;
    }
    else
        return {-1., -1., 2., 2.};
}

void CommentBlockView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if(event->button() == Qt::MouseButton::LeftButton)
    {
        //emit m_presenter.pressed(event->scenePos());
        m_presenter.setPressed(true);
        m_presenter.pressed(event->scenePos() - this->pos());
    }
}

void CommentBlockView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_presenter.moved(event->scenePos() - m_presenter.pressedPoint());
}

void CommentBlockView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_presenter.released(event->scenePos() - m_presenter.pressedPoint());
    m_presenter.setPressed(false);
}

