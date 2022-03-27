#include "GraphicsGridLayout.hpp"

#include <QPainter>
#include <QPen>

namespace score
{
GraphicsGridLayout::~GraphicsGridLayout()
{
    
}

void GraphicsGridLayout::setColumns(int columns)
{
  m_columns = columns;
}

void GraphicsGridLayout::layout()
{
  double cell_w = 0.;
  double cell_h = 0.;

  const auto items = this->childItems();

  // Compute the largest dimensions,
  // which will be the dimensions of a cell
  for(auto item : items)
  {
    const auto r = item->boundingRect();
    cell_w = std::max(cell_w, r.width());
    cell_h = std::max(cell_h, r.height());
  }
  cell_w += m_padding;
  cell_h += m_padding;

  // Layout
  int row = 0;
  int col = 0;
  for(auto item : items)
  {
    // Layout item in a cell
    auto cell = new score::EmptyRectItem{this};
    cell->setRect({0., 0., cell_w, cell_h});
    cell->setPos(m_padding + col * cell_w, m_padding + row * cell_h);
    item->setParentItem(cell);
    auto r = item->boundingRect();
    double item_x = (cell_w - r.width()) / 2.;
    double item_y = (cell_h - r.height()) / 2.;
    item->setPos(item_x, item_y);



    // Update positioning
    col++;
    if(col >= m_columns)
    {
      col = 0;
      row++;
    }
  }
/*
  // Make them fit
  for(auto item : items)
  {
    if(auto it = qgraphicsitem_cast<score::BackgroundItem*>(item))
    {
      QRectF cur = it->rect();
      cur.setHeight(cell_h);
      it->setRect(cur);
    }
  }*/
}
}
