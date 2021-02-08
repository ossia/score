#include <score/graphics/widgets/QGraphicsButton.hpp>
#include <score/model/Skin.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QTimer>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsButton);

namespace score
{
QGraphicsButton::QGraphicsButton(QGraphicsItem* parent)
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
}

void QGraphicsButton::bang()
{
  m_pressed = true;
  QTimer::singleShot(32, this, [this] { m_pressed = false; update(); });
  update();
}


void QGraphicsButton::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_pressed = true;
  pressed(true);
  update();
  event->accept();
}

void QGraphicsButton::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  m_pressed = false;
  pressed(false);
  update();
  event->accept();
}

void QGraphicsButton::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  auto& skin = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);

  constexpr const double margin = 2.;
  constexpr const  double backgroundRectWidth = 16.;

  constexpr const double insideCircleWidth = 12.;
  const double insideCircleOffset = margin + 0.5f * (backgroundRectWidth - insideCircleWidth);

  painter->setPen(skin.NoPen);
  painter->setBrush(!m_pressed ? skin.Emphasis2.main.brush : skin.Base4.main.brush);

  painter->drawEllipse(QRectF{margin, margin, backgroundRectWidth, backgroundRectWidth});

  if(m_pressed)
  {
    painter->setPen(skin.Emphasis2.main.pen2);
    painter->setBrush(skin.NoBrush);
    painter->drawEllipse(QRectF{insideCircleOffset, insideCircleOffset, insideCircleWidth, insideCircleWidth});
  }

  painter->setRenderHint(QPainter::Antialiasing, false);
}

QRectF QGraphicsButton::boundingRect() const
{
  return m_rect;
}
}
