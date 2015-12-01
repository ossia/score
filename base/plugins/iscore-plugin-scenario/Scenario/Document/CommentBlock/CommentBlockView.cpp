#include "CommentBlockView.hpp"

#include <QPainter>
#include <QWidget>
#include <QGraphicsTextItem>
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
}

void CommentBlockView::paint(QPainter* painter,
                             const QStyleOptionGraphicsItem* option,
                             QWidget* widget)
{

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

