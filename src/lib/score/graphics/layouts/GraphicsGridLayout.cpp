#include "GraphicsGridLayout.hpp"

#include <QPainter>
#include <QPen>

namespace score
{
GraphicsGridColumnsLayout::~GraphicsGridColumnsLayout()
{
    
}

void GraphicsGridColumnsLayout::setColumns(int columns)
{
  m_columns = columns;
}

void GraphicsGridColumnsLayout::layout()
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

    // Center the item in the cell
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

  // Make them fit
  for(auto item : items)
  {
    if(auto it = qgraphicsitem_cast<score::GraphicsLayout*>(item))
    {
      QRectF cur = it->rect();
      cur.setHeight(cell_h);
      it->setRect(cur);
    }
  }
}




GraphicsGridRowsLayout::~GraphicsGridRowsLayout()
{

}

void GraphicsGridRowsLayout::setRows(int rows)
{
  m_rows = rows;
}

void GraphicsGridRowsLayout::layout()
{
  double cell_w = 0.;
  double cell_h = 0.;

  m_padding = 0;
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
  int col = 0;
  int row = 0;
  for(auto item : items)
  {
    // Layout item in a cell
    auto cell = new score::EmptyRectItem{this};
    cell->setRect({0., 0., cell_w, cell_h});
    cell->setPos(m_padding + col * cell_w, m_padding + row * cell_h);
    item->setParentItem(cell);

    // Center the item in the cell
    auto r = item->boundingRect();
    double item_x = (cell_w - r.width()) / 2.;
    double item_y = (cell_h - r.height()) / 2.;
    item->setPos(item_x, item_y);

    // Update positioning
    row++;
    if(row >= m_rows)
    {
      row = 0;
      col++;
    }
  }

  // Make them fit
  for(auto item : items)
  {
    if(auto it = qgraphicsitem_cast<score::GraphicsLayout*>(item))
    {
      QRectF cur = it->rect();
      cur.setHeight(cell_h);
      it->setRect(cur);
    }
  }
}




GraphicsDefaultLayout::~GraphicsDefaultLayout()
{

}

static const constexpr int MaxRowsInEffect = 5;

// TODO not very efficient since it recomputes everything every time...
// Does a grid layout with maximum N rows per column.
template <typename F>
static QPointF currentWidgetPos(int controlIndex, F getControlSize) noexcept(
    noexcept(getControlSize(0)))
{
  int N = MaxRowsInEffect * (controlIndex / MaxRowsInEffect);
  qreal x = 0;
  for (int i = 0; i < N;)
  {
    qreal w = 0;
    for (int j = i; j < i + MaxRowsInEffect && j < N; j++)
    {
      auto sz = getControlSize(j);
      w = std::max(w, sz.width());
      w += default_padding;
    }
    x += w;
    i += MaxRowsInEffect;
  }

  qreal y = 0;
  for (int j = N; j < controlIndex; j++)
  {
    auto sz = getControlSize(j);
    y += sz.height() + default_padding;
  }

  return {x + default_padding, y + default_padding};
}
void GraphicsDefaultLayout::layout()
{
  const auto items = this->childItems();
  for(int i = 0; i < items.size(); i++)
  {
    auto it = items[i];
    it->setPos(currentWidgetPos(i, [&] (int j) { return items[j]->boundingRect().size(); }));
  }
}
}
