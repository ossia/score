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
void QGraphicsPixmapEnum::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  painter->setRenderHint(QPainter::Antialiasing, false);
  auto& style = score::Skin::instance();

  const QBrush& bg = style.NoBrush;
  const QPen& noPen = style.NoPen;

  int actual_rows = std::ceil(double(array.size()) / columns);
  int row = 0;
  int col = 0;
  const double w = m_smallRect.width() / columns;
  const double h = m_smallRect.height() / actual_rows;

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
