#include <score/graphics/widgets/QGraphicsTextButton.hpp>
#include <score/model/Skin.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QTimer>

#include <wobjectimpl.h>

W_OBJECT_IMPL(score::QGraphicsTextButton);
namespace score {


QGraphicsTextButton::QGraphicsTextButton(QGraphicsItem *parent,QString text)
: SimpleTextItem{score::Skin::instance().Base1.main, parent}
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
    pressed(true);
    update();
    event->accept();

}
void QGraphicsTextButton::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    m_pressed = true;
    pressed(true);
    update();
    event->accept();

}

void QGraphicsTextButton::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  m_pressed = false;
  pressed(false);
  update();
  event->accept();
}

void QGraphicsTextButton::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  painter->fillRect(boundingRect(),Qt::blue);
  auto& skin = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);

  constexpr const double margin = 2.;
  const double insideCircleWidth = m_rect.width();
  const double backgroundRectWidth = insideCircleWidth + 2*margin;

  const double insideCircleOffset
      = margin + 0.5f * (backgroundRectWidth - insideCircleWidth);

  painter->setPen(skin.NoPen);
  painter->setBrush(
      !m_pressed ? skin.Emphasis2.main.brush : skin.Base4.main.brush);

  painter->drawRect(m_rect);
//  painter->drawEllipse(
//      QRectF{margin, margin, backgroundRectWidth, backgroundRectWidth});

  if (m_pressed)
  {
//    painter->setPen(skin.Emphasis2.main.pen2);
    painter->fillRect(m_rect,Qt::blue);
  }

  painter->setRenderHint(QPainter::Antialiasing, false);
}


}
