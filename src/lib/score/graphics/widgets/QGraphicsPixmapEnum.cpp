#include <score/graphics/widgets/QGraphicsPixmapEnum.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Debug.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <cmath>
#include <wobjectimpl.h>

namespace score
{

QGraphicsPixmapEnum::QGraphicsPixmapEnum(QGraphicsItem* parent)
    : QGraphicsEnum{parent}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

void QGraphicsPixmapEnum::setupDefaultColumns(int N)
{
  switch(N)
  {
    case 0:
    case 1:
      this->columns = 1;
      this->rows = 1;
      break;
    case 2:
    case 3:
    case 5:
      this->columns = N;
      this->rows = 1;
      break;
    case 4:
      this->columns = 2;
      this->rows = 2;
      break;
    case 6:
      this->columns = 3;
      this->rows = 2;
      break;
    case 8:
      this->columns = 4;
      this->rows = 2;
      break;
    case 9:
      this->columns = 3;
      this->rows = 3;
      break;
    case 10:
      this->columns = 5;
      this->rows = 2;
      break;
    case 16:
      this->columns = 4;
      this->rows = 4;
      break;
    default:
      this->columns = std::ceil(double(N) / 2. + 0.5);
      this->rows = 2;
      break;
  }

  updateRect();
}

void QGraphicsPixmapEnum::updateRect()
{
  const int N = std::ssize(this->on_images);
  // Find the widest text
  if(columns == 0 || N == 0)
    return;

  m_actualColumns = std::min(N, columns);
  auto& style = score::Skin::instance();

  const QFont& textFont = style.MonoFontSmall;

  QFontMetricsF metrics{textFont};
  double maxW = 10.;
  double maxH = N > columns ? 20. : 15;

  for(auto& value : this->on_images)
  {
    maxW = std::max((double)value.width(), maxW);
    maxH = std::max((double)value.height(), maxH);
  }
  maxW += 4.;
  maxH += 4.;

  m_actualRows = N < columns ? 1 : std::max(1.0, std::ceil(double(N) / columns));
  setRect({0, 0, maxW * m_actualColumns, maxH * m_actualRows});
}

void QGraphicsPixmapEnum::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& style = score::Skin::instance();

  const QBrush& bg = style.NoBrush;
  const QPen& noPen = style.NoPen;

  int row = 0;
  int col = 0;
  const double w = m_smallRect.width() / m_actualColumns;
  const double h = m_smallRect.height() / m_actualRows;

  painter->setBrush(bg);
  int i = 0;
  for(std::size_t img = 0; img < off_images.size(); img++)
  {
    QRectF rect{2. + col * w, 2. + row * h, w, h};
    if(i != m_clicking)
    {
      painter->setPen(noPen);
      painter->drawRect(rect);
    }

    const QPixmap& pixmap = i != m_value ? on_images[img] : off_images[img];
    painter->drawPixmap(rect.topLeft(), pixmap);
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
