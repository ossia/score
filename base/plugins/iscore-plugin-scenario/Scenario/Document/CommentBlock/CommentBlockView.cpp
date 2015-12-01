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

    m_textItem = new QGraphicsTextItem{"Hello", this};
    m_textItem->setTextInteractionFlags(Qt::TextEditable);
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
        return m_textItem->boundingRect();
    else
        return {-1., -1., 2., 2.};
}

void CommentBlockView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if(event->button() == Qt::MouseButton::LeftButton)
        emit m_presenter.pressed(event->scenePos());
}

void CommentBlockView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_presenter.moved(event->scenePos());
}

void CommentBlockView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_presenter.released(event->scenePos());
}

