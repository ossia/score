#include <score/graphics/widgets/QGraphicsPixmapEnum.hpp>
#include <score/model/Skin.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>

namespace score
{

QGraphicsPixmapEnum::QGraphicsPixmapEnum(QGraphicsItem* parent) : QGraphicsEnum{parent}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
}

void QGraphicsPixmapEnum::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  painter->setRenderHint(QPainter::Antialiasing, false);
  auto& style = score::Skin::instance();

  const QBrush& bg = style.SliderBrush;
  const QPen& noPen = style.NoPen;

  int actual_rows = std::ceil(double(array.size()) / columns);
  int row = 0;
  int col = 0;
  const double w = m_smallRect.width() / columns;
  const double h = m_smallRect.height() / actual_rows;

  painter->setBrush(bg);
  int i = 0;
  for (std::size_t img = 0; img < off_images.size(); img++)
  {
    QRectF rect{2. + col * w, 2. + row * h, w, h};
    if (i != m_clicking)
    {
      painter->setPen(noPen);
      painter->drawRect(rect);
    }

    const QPixmap& pixmap = i != m_value ? on_images[img] : off_images[img];
    painter->drawPixmap(rect.topLeft(), pixmap);
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
