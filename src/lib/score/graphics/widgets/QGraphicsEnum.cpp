#include <score/graphics/widgets/QGraphicsEnum.hpp>
#include <score/model/Skin.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsEnum);

namespace score
{

QGraphicsEnum::QGraphicsEnum(QGraphicsItem* parent) : QGraphicsItem{parent}
{
  this->setAcceptedMouseButtons(Qt::LeftButton);
  setRect({0, 0, 104, 44});
}

void QGraphicsEnum::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
  m_smallRect = r.adjusted(2, 2, -2, -2);
}

void QGraphicsEnum::setValue(int v)
{
  m_value = ossia::clamp(v, 0, (int)array.size() - 1);
  update();
}

int QGraphicsEnum::value() const
{
  return m_value;
}

void QGraphicsEnum::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  int row = 0;
  int col = 0;
  int actual_rows = std::ceil(double(array.size()) / columns);
  const double w = m_smallRect.width() / columns;
  const double h = m_smallRect.height() / actual_rows;

  for (std::size_t i = 0; i < array.size(); i++)
  {
    QRectF rect{2. + col * w, 2. + row * h, w,h};
    if (rect.contains(event->pos()))
    {
      m_clicking = i;
      update();
      return;
    }

    col++;
    if (col == columns)
    {
      row++;
      col = 0;
    }
  }

  m_clicking = -1;
  update();
}

void QGraphicsEnum::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void QGraphicsEnum::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  if (m_clicking != -1)
  {
    m_value = m_clicking;
    m_clicking = -1;
    currentIndexChanged(m_value);
  }
  update();
}

QRectF QGraphicsEnum::boundingRect() const
{
  return m_rect;
}

void QGraphicsEnum::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  painter->setRenderHint(QPainter::Antialiasing, false);
  auto& style = score::Skin::instance();

  const QPen& text = style.Gray.main.pen1;
  const QFont& textFont = style.MonoFontSmall;
  const QPen& currentText = style.Base4.lighter180.pen1;
  const QBrush& bg = style.SliderBrush;
  const QPen& noPen = style.NoPen;

  int actual_rows = std::ceil(double(array.size()) / columns);
  int row = 0;
  int col = 0;
  const double w = m_smallRect.width() / columns;
  const double h = m_smallRect.height() / actual_rows;
  painter->setBrush(bg);
  int i = 0;
  QRectF clickRect{};
  for (const QString& str : array)
  {
    QRectF rect{2. + col * w, 2. + row * h, w, h};
    if (i == m_clicking)
    {
      clickRect = rect;
      painter->setPen(currentText);
    }
    else
    {
      painter->setPen(noPen);
      painter->drawRect(rect);
      painter->setPen(i != m_value ? text : currentText);
    }

    painter->setFont(textFont);
    painter->drawText(rect, str, QTextOption(Qt::AlignCenter));
    col++;
    if (col == columns)
    {
      row++;
      col = 0;
    }
    i++;
  }
}

}
