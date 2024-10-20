#include <score/graphics/widgets/QGraphicsEnum.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Debug.hpp>

#include <ossia/detail/math.hpp>
#include <ossia/detail/ssize.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsEnum);

namespace score
{

QGraphicsEnum::QGraphicsEnum(QGraphicsItem* parent)
    : QGraphicsItem{parent}
{
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
  setRect({0, 0, 144, 44});
}

void QGraphicsEnum::updateRect()
{
  const int N = std::ssize(this->array);
  // Find the widest text
  if(columns == 0 || N == 0)
    return;

  m_actualColumns = std::min(N, columns);
  auto& style = score::Skin::instance();

  const QFont& textFont = style.MonoFontSmall;

  QFontMetricsF metrics{textFont};
  double maxW = 10.;
  double maxH = N > columns ? 44. : 30;

  for(auto& value : this->array)
  {
    auto r = metrics.boundingRect(value);
    maxW = std::max(r.width(), maxW);
    maxH = std::max(r.height(), maxH);
  }
  maxW += 4.;
  maxH += 4.;

  m_actualRows = N < columns ? 1 : std::max(1.0, std::floor(N / columns));
  setRect({0, 0, maxW * m_actualColumns, maxH * m_actualRows});
}

void QGraphicsEnum::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
  m_smallRect = r.adjusted(2, 2, -2, -2);
}

void QGraphicsEnum::setOneLineRect() { }

void QGraphicsEnum::setValue(int32_t v)
{
  const int32_t sz = std::ssize(array);
  SCORE_ASSERT(sz > 0);
  m_value = ossia::clamp(v, 0, sz - 1);
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
  const double w = m_smallRect.width() / m_actualColumns;
  const double h = m_smallRect.height() / m_actualRows;

  for(std::size_t i = 0; i < array.size(); i++)
  {
    QRectF rect{2. + col * w, 2. + row * h, w, h};
    if(rect.contains(event->pos()))
    {
      m_clicking = i;
      update();
      return;
    }

    col++;
    if(col == columns)
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
  if(m_clicking != -1)
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
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& style = score::Skin::instance();

  const QPen& text = style.Gray.main.pen1;
  const QFont& textFont = style.MonoFontSmall;
  const QPen& currentText = style.Base4.main.pen1;
  const QBrush& bg = style.TransparentBrush;
  const QPen& noPen = style.NoPen;

  int row = 0;
  int col = 0;
  const double w = m_smallRect.width() / m_actualColumns;
  const double h = m_smallRect.height() / m_actualRows;
  painter->setBrush(bg);
  int i = 0;
  QRectF clickRect{};
  for(const QString& str : array)
  {
    QRectF rect{2. + col * w, 2. + row * h, w, h};
    if(i == m_clicking)
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
    if(col == columns)
    {
      row++;
      col = 0;
    }
    i++;
  }
}

}
