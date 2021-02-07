#include <score/graphics/widgets/QGraphicsCheckBox.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsCheckBox);

namespace score
{
QGraphicsCheckBox::QGraphicsCheckBox( QGraphicsItem* parent)
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
}

void QGraphicsCheckBox::toggle()
{
  m_toggled = !m_toggled;
  update();
}

void QGraphicsCheckBox::setState(bool toggled)
{
  if (toggled != m_toggled)
  {
    m_toggled = toggled;
    update();
  }
}

void QGraphicsCheckBox::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_toggled = !m_toggled;
  toggled(m_toggled);
  event->accept();
  update();
}

void QGraphicsCheckBox::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void QGraphicsCheckBox::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  auto& skin = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);

  constexpr const double checkBoxWidth = 12.;
  constexpr const double insideBoxWidth = 6.;

  double positionCheckBox = (m_rect.width() - checkBoxWidth) * 0.5;
  painter->fillRect(QRectF{positionCheckBox,positionCheckBox, checkBoxWidth, checkBoxWidth}, skin.Emphasis2.main.brush);

  if(m_toggled)
  {
    double position = (m_rect.width() - insideBoxWidth) * 0.5;
    painter->setPen(skin.Base4.main.pen2);
    painter->drawLine(position, position, position + insideBoxWidth, position + insideBoxWidth);
    painter->drawLine(position, position + + insideBoxWidth, position + + insideBoxWidth, position);
  }

  painter->setRenderHint(QPainter::Antialiasing, false);
 }

QRectF QGraphicsCheckBox::boundingRect() const
{
  return m_rect;
}
}
