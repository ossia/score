#include "GraphicsGridLayout.hpp"

#include <score/graphics/layouts/Constants.hpp>

#include <QPainter>
#include <QPen>

namespace score
{
GraphicsGridColumnsLayout::~GraphicsGridColumnsLayout() { }

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
    if(auto it = dynamic_cast<score::GraphicsLayout*>(item))
    {
      QRectF cur = it->rect();
      cur.setHeight(cell_h);
      it->setRect(cur);
    }
  }
}

GraphicsGridRowsLayout::~GraphicsGridRowsLayout() { }

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
    if(auto it = dynamic_cast<score::GraphicsLayout*>(item))
    {
      QRectF cur = it->rect();
      cur.setHeight(cell_h);
      it->setRect(cur);
    }
  }
}

GraphicsDefaultLayout::~GraphicsDefaultLayout() { }

static const constexpr int MaxRowsInEffect = 5;

// TODO not very efficient since it recomputes everything every time...
// Does a grid layout with maximum N rows per column.
template <typename F>
static QPointF currentWidgetPos(int controlIndex, F getControlSize) noexcept(
    noexcept(getControlSize(0)))
{
  int N = MaxRowsInEffect * (controlIndex / MaxRowsInEffect);
  qreal x = 0;
  for(int i = 0; i < N;)
  {
    qreal w = 0;
    for(int j = i; j < i + MaxRowsInEffect && j < N; j++)
    {
      auto sz = getControlSize(j);
      w = std::max(w, sz.width());
      w += default_padding;
    }
    x += w;
    i += MaxRowsInEffect;
  }

  qreal y = 0;
  for(int j = N; j < controlIndex; j++)
  {
    auto sz = getControlSize(j);
    y += sz.height() + default_padding;
  }

  return {x + default_padding, y + default_padding};
}

template <typename F>
static QPointF currentWidgetPos_outlets(int controlIndex, F getControlSize) noexcept(
    noexcept(getControlSize(0)))
{
  int N = MaxRowsInEffect * (controlIndex / MaxRowsInEffect);
  qreal x = 0;
  for(int i = 0; i < N;)
  {
    qreal w = 0;
    for(int j = i; j < i + MaxRowsInEffect && j < N; j++)
    {
      auto sz = getControlSize(j);
      w = std::max(w, sz.width());
      w += default_padding;
    }
    x += w;
    i += MaxRowsInEffect;
  }

  qreal y = 0;
  for(int j = N; j < controlIndex; j++)
  {
    auto sz = getControlSize(j);
    y += sz.height() + default_padding;
  }

  return {x + default_padding, y + default_padding};
}

void GraphicsDefaultLayout::layout()
{
  const auto items = this->childItems();
  updateChildrenRects(items);

  for(int i = 0; i < items.size(); i++)
  {
    auto it = items[i];
    it->setPos(
        currentWidgetPos(i, [&](int j) { return items[j]->boundingRect().size(); }));
  }
}

GraphicsDefaultInletLayout::~GraphicsDefaultInletLayout() { }
void GraphicsDefaultInletLayout::layout()
{
  const auto items = this->childItems();
  updateChildrenRects(items);
  double cur_x = 0;
  double cur_y = 0;

  for(int i = 0; i < items.size(); i++)
  {
    auto it = items[i];
    auto rect = it->boundingRect();
    it->setPos(cur_x, cur_y);
    cur_y += rect.height();
  }
}

GraphicsDefaultOutletLayout::~GraphicsDefaultOutletLayout() { }
void GraphicsDefaultOutletLayout::layout()
{
  const auto items = this->childItems();
  updateChildrenRects(items);
  // 1. Find longest item
  double w = 0.;
  for(int i = 0; i < items.size(); i++)
    if(auto ww = items[i]->boundingRect().width(); ww > w)
      w = ww;

  double cur_x = 0;
  double cur_y = 0;

  for(int i = 0; i < items.size(); i++)
  {
    auto it = items[i];
    auto rect = it->boundingRect();
    cur_x = w - rect.width();
    it->setPos(cur_x, cur_y);
    cur_y += rect.height();
  }
}

GraphicsIORootLayout::~GraphicsIORootLayout() { }
void GraphicsIORootLayout::setMinimumWidth(double w)
{
  m_minimumWidth = w;
}
void GraphicsIORootLayout::layout()
{
  const auto items = this->childItems();
  if(items.size() == 2)
  {
    updateChildrenRects(items);
    items[0]->setPos(0, 0);
    auto inlets_w = items[0]->boundingRect().width();
    auto outlets_w = items[1]->boundingRect().width();
    if(inlets_w + 5. + outlets_w > m_minimumWidth)
      items[1]->setPos(inlets_w + 5., 0);
    else
      items[1]->setPos(m_minimumWidth - outlets_w, 0);
  }
  else if(items.size() == 3)
  {
    updateChildrenRects(items);
    items[0]->setPos(0, 0);
    auto no_control_inlets_w = items[0]->boundingRect().width();
    auto control_inlets_w = items[1]->boundingRect().width();
    items[1]->setPos(no_control_inlets_w + 5., 0.);

    auto outlets_w = items[2]->boundingRect().width();
    if(no_control_inlets_w + control_inlets_w + 10. + outlets_w > m_minimumWidth)
      items[2]->setPos(no_control_inlets_w + control_inlets_w + 10., 0);
    else
      items[2]->setPos(m_minimumWidth - outlets_w, 0);
  }
}
}
