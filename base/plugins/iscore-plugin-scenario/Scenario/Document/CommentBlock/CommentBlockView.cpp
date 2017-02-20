#include "CommentBlockView.hpp"
#include "TextItem.hpp"

#include <QFont>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QTextCursor>
#include <QTextDocument>
#include <QWidget>

#include <Scenario/Document/CommentBlock/CommentBlockPresenter.hpp>
#include <cmath>
namespace Scenario
{
CommentBlockView::CommentBlockView(
    CommentBlockPresenter& presenter, QQuickPaintedItem* parent)
    : GraphicsItem{parent}, m_presenter{presenter}
{
  this->setParentItem(parent);
  this->setZ(ZPos::Comment);
  this->setAcceptHoverEvents(true);

  m_textItem = new TextItem{"", this};
/*
  connect(
      m_textItem->document(), &QTextDocument::contentsChanged, this,
      [&]() { this->prepareGeometryChange(); });
  connect(m_textItem, &TextItem::focusOut, this, &CommentBlockView::focusOut);
  focusOut();
  */
}

void CommentBlockView::paint(
    QPainter* painter)
{
  auto p = QPen{Qt::white};
  p.setWidth(1.);
  painter->setPen(p);
  painter->drawRoundedRect(boundingRect(), 5, 5);
}

QRectF CommentBlockView::boundingRect() const
{
  if (m_textItem)
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
  if (m_selected == b)
    return;

  m_selected = b;
}

void CommentBlockView::setHtmlContent(QString htmlText)
{
  //m_textItem->setHtml(htmlText);
}

void CommentBlockView::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::MouseButton::LeftButton)
  {
    //m_clickedPoint = mapToScene(event->localPos()) - this->pos();
    m_clickedScenePoint = mapToScene(event->localPos());
  }
}

void CommentBlockView::mouseMoveEvent(QMouseEvent* event)
{
  emit m_presenter.moved(mapToScene(event->localPos()) - m_clickedPoint);
}

void CommentBlockView::mouseReleaseEvent(QMouseEvent* event)
{
  auto p = mapToScene(event->localPos());
  auto d = (m_clickedScenePoint - p).manhattanLength();
  if (std::abs(d) < 5)
    emit m_presenter.selected();

  emit m_presenter.released(mapToScene(event->localPos()));
}

void CommentBlockView::mouseDoubleClickEvent(QMouseEvent* evt)
{
  focusOnText();
}

void CommentBlockView::focusOnText()
{
  /*
  if (m_textItem->textInteractionFlags() == Qt::NoTextInteraction)
  {
    m_textItem->setTextInteractionFlags(Qt::TextEditorInteraction);
    m_textItem->setFocus(Qt::MouseFocusReason);
    QTextCursor c = m_textItem->textCursor();
    c.select(QTextCursor::Document);
    m_textItem->setTextCursor(c);
  }
  */
}

void CommentBlockView::focusOut()
{
  /*
  m_textItem->setTextInteractionFlags(Qt::NoTextInteraction);
  QTextCursor c = m_textItem->textCursor();
  c.clearSelection();
  m_textItem->setTextCursor(c);
  clearFocus();
  emit m_presenter.editFinished(m_textItem->toHtml());
  */
}
}
