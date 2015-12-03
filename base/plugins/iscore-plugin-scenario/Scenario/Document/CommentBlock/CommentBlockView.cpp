#include "CommentBlockView.hpp"

#include <QPainter>
#include <QWidget>
#include <QFont>
#include <QGraphicsSceneMouseEvent>
#include <QTextCursor>
#include <QTextDocument>

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

    m_textItem = new QGraphicsTextItem{"", this};
    m_textItem->setDefaultTextColor(Qt::white);

    connect(m_textItem->document(), &QTextDocument::contentsChanged,
            this, [&] () {this->prepareGeometryChange();});
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

void CommentBlockView::setSelected(bool b)
{
    if(m_selected == b)
        return;

    m_selected = b;
    SetTextInteraction(b);
}

void CommentBlockView::setHtmlContent(QString htmlText)
{
    m_textItem->setHtml(htmlText);
}

void CommentBlockView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if(event->button() == Qt::MouseButton::LeftButton)
    {
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
    emit m_presenter.released(event->scenePos());
    m_presenter.setPressed(false);
}

void CommentBlockView::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* evt)
{
    emit m_presenter.doubleClicked();
}

void CommentBlockView::SetTextInteraction(bool on, bool selectAll)
{
    if(on && m_textItem->textInteractionFlags() == Qt::NoTextInteraction)
    {
        m_textItem->setTextInteractionFlags(Qt::TextEditorInteraction);
        m_textItem->setFocus(Qt::MouseFocusReason);
    }
    else if(!on && m_textItem->textInteractionFlags() == Qt::TextEditorInteraction)
    {
        m_textItem->setTextInteractionFlags(Qt::NoTextInteraction);
        QTextCursor c = m_textItem->textCursor();
        c.clearSelection();
        m_textItem->setTextCursor(c);
        clearFocus();
        emit m_presenter.editFinished(m_textItem->document()->toHtml());
    }
}

