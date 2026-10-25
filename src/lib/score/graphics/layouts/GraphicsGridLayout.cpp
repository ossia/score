#include "GraphicsGridLayout.hpp"

#include <algorithm>

#include <score/graphics/layouts/Constants.hpp>
#include <score/graphics/layouts/GraphicsBoxLayout.hpp>
#include <score/model/Skin.hpp>

#include <QEvent>
#include <QFontMetricsF>
#include <QPainter>
#include <QPen>

#include <utility>
#include <vector>

namespace score
{
namespace
{
//! Wraps an item of a grid, to place it in its cell
class GraphicsGridCell final : public EmptyRectItem
{
public:
  using EmptyRectItem::EmptyRectItem;
};
}

//! The grid's items, out of the cells of a previous layout, in order:
//! laying out the cells themselves would grow them by the gap each time.
static QList<QGraphicsItem*> takeGridItems(QGraphicsItem& grid)
{
  const auto children = grid.childItems();
  QList<QGraphicsItem*> items;
  items.reserve(children.size());
  for(auto child : children)
  {
    if(auto cell = dynamic_cast<GraphicsGridCell*>(child))
    {
      for(auto item : cell->childItems())
      {
        item->setParentItem(&grid);
        items.push_back(item);
      }
      delete cell;
    }
    else
    {
      items.push_back(child);
    }
  }
  return items;
}

//! Cells of a column as wide as its widest item, of a row as tall as its
//! tallest, items aligned in their column and centred in their row.
//! gap: after each cell, as the padding is in the uniform grid.
template <typename CellOf>
static void layoutAlignedGridCells(
    GraphicsLayout& grid, const QList<QGraphicsItem*>& items, qreal padding,
    qreal gap, CellOf cellOf)
{
  std::vector<double> col_w;
  std::vector<double> row_h;
  for(int i = 0; i < items.size(); i++)
  {
    const auto [row, col] = cellOf(i);
    if(std::ssize(col_w) <= col)
      col_w.resize(col + 1, 0.);
    if(std::ssize(row_h) <= row)
      row_h.resize(row + 1, 0.);
    const auto r = items[i]->boundingRect();
    col_w[col] = std::max(col_w[col], r.width());
    row_h[row] = std::max(row_h[row], r.height());
  }

  std::vector<double> col_x(col_w.size(), padding);
  for(std::size_t c = 1; c < col_w.size(); c++)
    col_x[c] = col_x[c - 1] + col_w[c - 1] + gap;
  std::vector<double> row_y(row_h.size(), padding);
  for(std::size_t r = 1; r < row_h.size(); r++)
    row_y[r] = row_y[r - 1] + row_h[r - 1] + gap;

  for(int i = 0; i < items.size(); i++)
  {
    const auto [row, col] = cellOf(i);
    auto item = items[i];
    auto cell = new GraphicsGridCell{&grid};
    cell->setRect({0., 0., col_w[col] + gap, row_h[row] + gap});
    cell->setPos(col_x[col], row_y[row]);
    item->setParentItem(cell);

    const auto r = item->boundingRect();
    item->setPos(
        alignedOffset(grid.alignment(), col_w[col], r.width()),
        (row_h[row] + gap - r.height()) / 2.);

    // Exactly the row span, else it overhangs the cell
    if(auto it = dynamic_cast<score::GraphicsLayout*>(item))
    {
      QRectF cur = it->rect();
      cur.setTop(-it->pos().y());
      cur.setHeight(row_h[row] + gap);
      it->setRect(cur);
    }
  }
}

GraphicsGridColumnsLayout::~GraphicsGridColumnsLayout() { }

void GraphicsGridColumnsLayout::setColumns(int columns)
{
  m_columns = columns;
}

void GraphicsGridColumnsLayout::layout()
{
  double cell_w = 0.;
  double cell_h = 0.;

  const auto items = takeGridItems(*this);
  if(m_alignment)
  {
    const int columns = std::max(1, m_columns);
    const qreal gap = m_spacing >= 0. ? m_spacing : m_padding;
    layoutAlignedGridCells(*this, items, m_padding, gap, [columns](int i) {
      return std::pair{i / columns, i % columns};
    });
    return;
  }

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
    auto cell = new GraphicsGridCell{this};
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

  // Make them fit: exactly the cell span, else it overhangs the cell
  for(auto item : items)
  {
    if(auto it = dynamic_cast<score::GraphicsLayout*>(item))
    {
      QRectF cur = it->rect();
      cur.setTop(-it->pos().y());
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
  const auto items = takeGridItems(*this);
  if(m_alignment)
  {
    const int rows = std::max(1, m_rows);
    const qreal gap = m_spacing >= 0. ? m_spacing : m_padding;
    layoutAlignedGridCells(*this, items, m_padding, gap, [rows](int i) {
      return std::pair{i % rows, i / rows};
    });
    return;
  }

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
    auto cell = new GraphicsGridCell{this};
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

  // Make them fit: exactly the cell span, else it overhangs the cell
  for(auto item : items)
  {
    if(auto it = dynamic_cast<score::GraphicsLayout*>(item))
    {
      QRectF cur = it->rect();
      cur.setTop(-it->pos().y());
      cur.setHeight(cell_h);
      it->setRect(cur);
    }
  }
}

GraphicsTableLayout::~GraphicsTableLayout() { }

void GraphicsTableLayout::setColumnTitles(QStringList titles)
{
  m_titles = std::move(titles);
}

void GraphicsTableLayout::setRowTitles(bool rowTitles)
{
  m_rowTitles = rowTitles;
}

void GraphicsTableLayout::setTitle(QString title)
{
  m_title = std::move(title);
}

void GraphicsTableLayout::setRowsSelectable(bool selectable)
{
  m_rowsSelectable = selectable;
  setFiltersChildEvents(selectable);
}

bool GraphicsTableLayout::sceneEventFilter(QGraphicsItem* watched, QEvent* event)
{
  // Background press, propagated by the row
  if(event->type() == QEvent::GraphicsSceneMousePress)
    if(auto row = dynamic_cast<GraphicsSelectableRow*>(watched))
      row->activate();
  return false;
}

QFont GraphicsTableLayout::titleFont()
{
  return score::Skin::instance().Medium8Pt;
}

void GraphicsTableLayout::layout()
{
  const auto rows = this->childItems();

  // Measure: each column is as wide as its widest cell
  std::vector<double> col_w;
  std::vector<double> row_h;
  for(auto row : rows)
  {
    const auto cells = row->childItems();
    updateChildrenRects(cells);
    double h = 0.;
    for(int c = 0; c < cells.size(); c++)
    {
      const auto r = cells[c]->boundingRect();
      if(std::ssize(col_w) <= c)
        col_w.push_back(0.);
      col_w[c] = std::max(col_w[c], r.width());
      h = std::max(h, r.height());
    }
    row_h.push_back(h);
  }

  // Titles count in column width
  const QFontMetricsF fm{titleFont()};
  const int first_titled = m_rowTitles ? 1 : 0;
  for(int t = 0; t < m_titles.size(); t++)
  {
    const int c = t + first_titled;
    if(std::ssize(col_w) <= c)
      col_w.push_back(0.);
    col_w[c] = std::max(col_w[c], fm.horizontalAdvance(m_titles[t]));
  }
  if(m_rowTitles && !m_title.isEmpty())
  {
    if(col_w.empty())
      col_w.push_back(0.);
    QFont tf = titleFont();
    tf.setCapitalization(QFont::AllUppercase);
    col_w[0] = std::max(col_w[0], QFontMetricsF{tf}.horizontalAdvance(m_title));
  }
  const bool has_header = !m_titles.isEmpty() || (m_rowTitles && !m_title.isEmpty());
  const double header_h = has_header ? fm.height() + 2. : 0.;

  m_columns.clear();
  double x = 0.;
  for(double w : col_w)
  {
    m_columns.emplace_back(x, w);
    x += w + spacing();
  }
  const double total_w = col_w.empty() ? 0. : x - spacing();

  // Inset in selectable rows for the highlight
  const double inset_x = m_rowsSelectable ? 6. : 0.;
  const double inset_y = m_rowsSelectable ? 2. : 0.;

  // Place: cells centred in their row, and in their column by default
  const Qt::Alignment align = m_alignment ? m_alignment : Qt::AlignHCenter;
  double y = m_padding + header_h;
  for(int r = 0; r < rows.size(); r++)
  {
    auto row = rows[r];
    const auto cells = row->childItems();
    for(int c = 0; c < cells.size(); c++)
    {
      const auto cr = cells[c]->boundingRect();
      cells[c]->setPos(
          inset_x + m_columns[c].first
              + alignedOffset(align, m_columns[c].second, cr.width()),
          inset_y + (row_h[r] - cr.height()) / 2.);
    }
    const double h = row_h[r] + 2. * inset_y;
    row->setPos(m_padding - inset_x, y);
    if(auto lay = dynamic_cast<score::GraphicsLayout*>(row))
    {
      // The columns place the cells: a row moving them would break them
      lay->setAlignment({});
      lay->setRect({0., 0., total_w + 2. * inset_x, h});
    }
    y += h + (r + 1 < rows.size() ? spacing() : 0.);
  }

  setRect({0., 0., total_w + 2. * m_padding, y + m_padding});
}

void GraphicsTableLayout::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  GraphicsLayout::paint(painter, option, widget);
  if(m_columns.empty())
    return;

  auto& skin = score::Skin::instance();
  QFont f = titleFont();
  const QFontMetricsF fm{f};
  painter->setPen(skin.LightGray.main.pen1);
  if(m_rowTitles && !m_title.isEmpty())
  {
    // Table title, above the row titles
    QFont tf = f;
    tf.setCapitalization(QFont::AllUppercase);
    painter->setFont(tf);
    painter->drawText(
        QRectF{m_padding + m_columns[0].first, m_padding, m_columns[0].second, fm.height()},
        m_title, QTextOption(Qt::AlignLeft | Qt::AlignVCenter));
  }
  painter->setFont(f);
  const int first_titled = m_rowTitles ? 1 : 0;
  for(int t = 0; t < m_titles.size(); t++)
  {
    const int c = t + first_titled;
    if(c >= std::ssize(m_columns))
      break;
    const auto [cx, cw] = m_columns[c];
    painter->drawText(
        QRectF{m_padding + cx, m_padding, cw, fm.height()}, m_titles[t],
        QTextOption((m_alignment ? m_alignment : Qt::AlignHCenter) | Qt::AlignVCenter));
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
void GraphicsDefaultOutletLayout::setMinimumWidth(double w)
{
  m_minimumWidth = w;
}
void GraphicsDefaultOutletLayout::layout()
{
  const auto items = this->childItems();
  updateChildrenRects(items);
  // 1. Find longest item
  double w = 0.;
  for(int i = 0; i < items.size(); i++)
    if(auto ww = items[i]->boundingRect().width(); ww > w)
      w = ww;

  // Right edge of the column: the node's width when it is the wider of the
  // two. A node with outlets and no inlets used to leave this at the column's
  // own width, which put its outlets against the LEFT edge of the node.
  const double right = std::max(w, m_minimumWidth);

  double cur_y = 0;

  for(int i = 0; i < items.size(); i++)
  {
    auto it = items[i];
    auto rect = it->boundingRect();
    it->setPos(right - rect.width(), cur_y);
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
