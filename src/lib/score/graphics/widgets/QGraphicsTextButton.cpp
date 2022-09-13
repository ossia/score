#include <score/graphics/widgets/QGraphicsTextButton.hpp>
#include <score/model/Skin.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QTextLayout>


#include <wobjectimpl.h>

W_OBJECT_IMPL(score::QGraphicsTextButton);
namespace score {


QGraphicsTextButton::QGraphicsTextButton(QString text,QGraphicsItem *parent)
  : QGraphicsItem{parent}
{
    setText(std::move(text));
    auto& skin = score::Skin::instance();
    updateBounds();
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

}
void QGraphicsTextButton::updateBounds(){
    auto& skin = score::Skin::instance();
    if(m_string.isEmpty())
        m_string = "Open File";
    QTextLayout layout(m_string,skin.Medium12Pt);
    layout.beginLayout();
    auto line = layout.createLine();
    layout.endLayout();

    m_rect = line.naturalTextRect();

}

QRectF QGraphicsTextButton::boundingRect() const
{
    return m_rect;

}

}
