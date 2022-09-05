#include <score/graphics/widgets/QGraphicsTextButton.hpp>
#include <score/model/Skin.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QTimer>

#include <wobjectimpl.h>

W_OBJECT_IMPL(score::QGraphicsTextButton);
namespace score {


QGraphicsTextButton::QGraphicsTextButton(QString text,QGraphicsItem *parent)
  : QGraphicsItem{parent}
{
    setText(std::move(text));
    auto& skin = score::Skin::instance();
    setCursor(skin.CursorPointingHand);

}
void QGraphicsTextButton::bang()
{
  m_pressed = true;
  QTimer::singleShot(32,this,[this]{
      m_pressed = false;
      update();
  });
  update();
}
void QGraphicsTextButton::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    m_pressed = true;
    pressed();
    update();
    event->accept();

}
void QGraphicsTextButton::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    m_pressed = true;
    pressed();
    update();
    event->accept();

}

void QGraphicsTextButton::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  m_pressed = false;
  pressed();
  update();
  event->accept();
}

void QGraphicsTextButton::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  auto& skin = score::Skin::instance();

  const QRectF brect = boundingRect().adjusted(1, 1, -1, -1);
  painter->drawRoundedRect(brect, 1, 1);

  if(!m_string.isEmpty())
  {
    painter->setPen(skin.Base4.main.pen2);
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setFont(skin.Medium10Pt);
    painter->drawText(brect, m_string, QTextOption(Qt::AlignCenter));
  }
  /*auto& skin = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);

  constexpr const double margin = 2.;
  const double insideCircleWidth = boundingRect().width();
  const double backgroundRectWidth = insideCircleWidth + 2*margin;

  const double insideCircleOffset
      = margin + 0.5f * (backgroundRectWidth - insideCircleWidth);

  painter->setPen(skin.NoPen);
  painter->setBrush(
      !m_pressed ? skin.Emphasis2.main.brush : skin.Base4.main.brush);

  painter->drawRect(boundingRect());

  if (m_pressed)
  {
    painter->drawRect(boundingRect());
  }
//  SimpleTextItem::paint(painter,option,widget);
  painter->drawText(m_rect.topLeft(),m_string);

  painter->setRenderHint(QPainter::Antialiasing, false);*/
}

QRectF QGraphicsTextButton::boundingRect() const
{
  return m_rect;
}

}
