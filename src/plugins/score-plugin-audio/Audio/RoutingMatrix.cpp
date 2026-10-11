#include "RoutingMatrix.hpp"

#include <score/model/Skin.hpp>

#include <QAbstractTableModel>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QStyledItemDelegate>

#include <algorithm>
#include <map>

namespace Audio
{
namespace
{
constexpr int connected_role = Qt::UserRole + 1;
constexpr int port_role = Qt::UserRole + 2;
constexpr int first_of_port_role = Qt::UserRole + 3;
constexpr int hovered_role = Qt::UserRole + 4;

QString ctrlName()
{
#if defined(Q_OS_MACOS)
  return QStringLiteral("⌘");
#else
  return QObject::tr("Ctrl+");
#endif
}

QString shiftName()
{
#if defined(Q_OS_MACOS)
  return QStringLiteral("⇧");
#else
  return QObject::tr("Shift+");
#endif
}
}

class RoutingModel final : public QAbstractTableModel
{
public:
  RoutingModel(RoutingMatrix::Direction d, QObject* parent)
      : QAbstractTableModel{parent}
      , m_direction{d}
  {
  }

  struct Row
  {
    int port{};
    int channel{};
  };

  std::vector<RoutedPort> ports;
  std::vector<Row> rows;
  int columns{};
  int hoveredRow{-1};
  int hoveredColumn{-1};

  int rowCount(const QModelIndex& parent) const override
  {
    return parent.isValid() ? 0 : int(rows.size());
  }
  int columnCount(const QModelIndex& parent) const override
  {
    return parent.isValid() ? 0 : columns;
  }

  int routeOf(int row) const noexcept
  {
    const auto [p, ch] = rows[row];
    const auto& m = ports[p].mapping;
    return ch < int(m.size()) ? m[ch] : unrouted;
  }

  QString rowName(int row) const
  {
    const auto [p, ch] = rows[row];
    return QStringLiteral("%1 · %2").arg(ports[p].name).arg(ch + 1);
  }

  QString cardChannelName(int col) const
  {
    return m_direction == RoutingMatrix::Direction::FromCard
               ? QObject::tr("Sound card input %1").arg(col + 1)
               : QObject::tr("Sound card output %1").arg(col + 1);
  }

  QString routeText(int row, int col) const
  {
    return m_direction == RoutingMatrix::Direction::FromCard
               ? QStringLiteral("%1 → %2").arg(cardChannelName(col), rowName(row))
               : QStringLiteral("%1 → %2").arg(rowName(row), cardChannelName(col));
  }

  QVariant data(const QModelIndex& index, int role) const override
  {
    if(!index.isValid() || index.row() >= int(rows.size()))
      return {};
    const int row = index.row();
    const int col = index.column();
    switch(role)
    {
      case connected_role:
        return routeOf(row) == col;
      case port_role:
        return rows[row].port;
      case first_of_port_role:
        return rows[row].channel == 0;
      case hovered_role:
        return row == hoveredRow || col == hoveredColumn;
      case Qt::ToolTipRole:
        return routeText(row, col);
      case Qt::AccessibleTextRole:
        return routeOf(row) == col
                   ? QObject::tr("%1, connected").arg(routeText(row, col))
                   : routeText(row, col);
    }
    return {};
  }

  QVariant headerData(int section, Qt::Orientation o, int role) const override
  {
    const bool horizontal = o == Qt::Horizontal;
    if(horizontal ? section >= columns : section >= int(rows.size()))
      return {};
    switch(role)
    {
      case Qt::DisplayRole:
        return horizontal ? QString::number(section + 1) : rowName(section);
      case Qt::ToolTipRole:
        if(horizontal)
          return cardChannelName(section);
        if(const int r = routeOf(section); r >= columns)
          return QObject::tr("%1, which the sound card does not have")
              .arg(routeText(section, r));
        else if(r != unrouted)
          return routeText(section, r);
        return QObject::tr("%1 goes to no channel of the sound card")
            .arg(rowName(section));
      case Qt::FontRole:
        if(section == (horizontal ? hoveredColumn : hoveredRow))
        {
          QFont f;
          f.setBold(true);
          return f;
        }
        return {};
      case Qt::ForegroundRole:
        if(!horizontal && (routeOf(section) < 0 || routeOf(section) >= columns))
          return QPalette{}.color(QPalette::PlaceholderText);
        return {};
    }
    return {};
  }

  Qt::ItemFlags flags(const QModelIndex& index) const override
  {
    return index.isValid() ? Qt::ItemIsEnabled : Qt::NoItemFlags;
  }

  void setPorts(std::vector<RoutedPort> p)
  {
    std::vector<Row> r;
    for(int i = 0; i < int(p.size()); i++)
      for(int ch = 0; ch < int(p[i].mapping.size()); ch++)
        r.push_back({i, ch});

    const bool same_shape
        = r.size() == rows.size() && p.size() == ports.size()
          && std::equal(p.begin(), p.end(), ports.begin(), [](auto& a, auto& b) {
      return a.name == b.name && a.mapping.size() == b.mapping.size();
    });
    if(same_shape)
    {
      ports = std::move(p);
      if(!rows.empty() && columns > 0)
      {
        dataChanged(index(0, 0), index(int(rows.size()) - 1, columns - 1));
        headerDataChanged(Qt::Vertical, 0, int(rows.size()) - 1);
      }
      return;
    }

    beginResetModel();
    ports = std::move(p);
    rows = std::move(r);
    hoveredRow = -1;
    hoveredColumn = -1;
    endResetModel();
  }

  void setDirection(RoutingMatrix::Direction d)
  {
    if(d == m_direction)
      return;
    beginResetModel();
    m_direction = d;
    endResetModel();
  }

  void setColumns(int c)
  {
    if(c == columns)
      return;
    beginResetModel();
    columns = c;
    hoveredColumn = -1;
    endResetModel();
  }

  bool setHovered(int row, int col)
  {
    if(row == hoveredRow && col == hoveredColumn)
      return false;
    const int old_row = hoveredRow;
    const int old_col = hoveredColumn;
    hoveredRow = row;
    hoveredColumn = col;
    for(int r : {old_row, row})
      if(r >= 0 && r < int(rows.size()))
        headerDataChanged(Qt::Vertical, r, r);
    for(int c : {old_col, col})
      if(c >= 0 && c < columns)
        headerDataChanged(Qt::Horizontal, c, c);
    return true;
  }

private:
  RoutingMatrix::Direction m_direction{};
};

namespace
{
class RoutingDelegate final : public QStyledItemDelegate
{
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx)
      const override
  {
    const auto& pal = opt.palette;
    const QRectF r = opt.rect;
    p->save();
    p->setRenderHint(QPainter::Antialiasing);

    p->fillRect(
        r, idx.data(port_role).toInt() % 2 ? pal.color(QPalette::AlternateBase)
                                           : pal.color(QPalette::Base));
    if(idx.data(hovered_role).toBool())
    {
      auto c = pal.color(QPalette::Highlight);
      c.setAlpha(40);
      p->fillRect(r, c);
    }
    if(idx.data(first_of_port_role).toBool() && idx.row() > 0)
    {
      p->setPen(QPen{pal.color(QPalette::Mid), 2.});
      p->drawLine(r.topLeft(), r.topRight());
    }
    if(idx.data(connected_role).toBool())
    {
      const double radius = std::min(r.width(), r.height()) * 0.3;
      p->setPen(Qt::NoPen);
      p->setBrush(score::Skin::instance().Port1.main.brush);
      p->drawEllipse(r.center(), radius, radius);
    }
    if(opt.state & QStyle::State_HasFocus)
    {
      p->setPen(QPen{pal.color(QPalette::Highlight), 1.});
      p->setBrush(Qt::NoBrush);
      p->drawRect(r.adjusted(1., 1., -1., -1.));
    }
    p->restore();
  }

  QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
  {
    return {24, 22};
  }
};
}

RoutingMatrix::RoutingMatrix(Direction d, QWidget* parent)
    : QTableView{parent}
    , m_model{new RoutingModel{d, this}}
{
  setModel(m_model);
  setItemDelegate(new RoutingDelegate{this});
  setSelectionMode(QAbstractItemView::NoSelection);
  setEditTriggers(QAbstractItemView::NoEditTriggers);
  setMouseTracking(true);
  setTabKeyNavigation(false);
  setFocusPolicy(Qt::StrongFocus);
  setCornerButtonEnabled(false);
  setWordWrap(false);

  auto h = horizontalHeader();
  h->setSectionResizeMode(QHeaderView::Fixed);
  h->setMinimumSectionSize(20);
  h->setDefaultSectionSize(26);
  h->setSectionsClickable(false);
  h->setSectionsMovable(false);
  h->setDefaultAlignment(Qt::AlignCenter);

  auto v = verticalHeader();
  v->setSectionResizeMode(QHeaderView::Fixed);
  v->setDefaultSectionSize(22);
  v->setSectionsClickable(false);
  v->setSectionsMovable(false);
  v->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

RoutingMatrix::~RoutingMatrix() = default;

void RoutingMatrix::setDirection(Direction d)
{
  m_model->setDirection(d);
}

void RoutingMatrix::setCardChannels(int count)
{
  m_model->setColumns(std::max(count, 0));
}

int RoutingMatrix::cardChannels() const noexcept
{
  return m_model->columns;
}

void RoutingMatrix::setPorts(std::vector<RoutedPort> ports)
{
  m_model->setPorts(std::move(ports));
}

const std::vector<RoutedPort>& RoutingMatrix::ports() const noexcept
{
  return m_model->ports;
}

QString RoutingMatrix::help()
{
  return QObject::tr(
             "Click a cell to connect a channel of a port to a channel of the sound "
             "card, and again to disconnect it. %1Click connects diagonally from that "
             "cell to the end of the port, %1%2Click to the end of the list. "
             "Right-click for more.")
      .arg(ctrlName(), shiftName());
}

void RoutingMatrix::route(Changes c)
{
  if(c.empty())
    return;
  if(onRoute)
  {
    onRoute(std::move(c));
    return;
  }
  auto ports = m_model->ports;
  for(auto& [p, m] : c)
    if(p >= 0 && p < int(ports.size()))
      ports[p].mapping = std::move(m);
  m_model->setPorts(std::move(ports));
}

void RoutingMatrix::toggle(int row, int column)
{
  if(row < 0 || row >= int(m_model->rows.size()) || column < 0
     || column >= m_model->columns)
    return;
  const auto [p, ch] = m_model->rows[row];
  auto m = m_model->ports[p].mapping;
  m[ch] = m[ch] == column ? unrouted : column;
  route({{p, std::move(m)}});
}

void RoutingMatrix::connectDiagonally(int row, int column, bool acrossPorts)
{
  const auto& rows = m_model->rows;
  if(row < 0 || row >= int(rows.size()) || column < 0 || column >= m_model->columns)
    return;
  const int first_port = rows[row].port;
  std::map<int, ossia::int_vector> changed;
  for(int r = row, c = column; r < int(rows.size()) && c < m_model->columns; r++, c++)
  {
    const auto [p, ch] = rows[r];
    if(!acrossPorts && p != first_port)
      break;
    auto it = changed.find(p);
    if(it == changed.end())
      it = changed.emplace(p, m_model->ports[p].mapping).first;
    it->second[ch] = c;
  }
  Changes res;
  for(auto& [p, m] : changed)
    if(m != m_model->ports[p].mapping)
      res.emplace_back(p, std::move(m));
  route(std::move(res));
}

void RoutingMatrix::disconnectRow(int row)
{
  if(row < 0 || row >= int(m_model->rows.size()))
    return;
  const auto [p, ch] = m_model->rows[row];
  auto m = m_model->ports[p].mapping;
  if(m[ch] == unrouted)
    return;
  m[ch] = unrouted;
  route({{p, std::move(m)}});
}

void RoutingMatrix::disconnectPort(int port)
{
  if(port < 0 || port >= int(m_model->ports.size()))
    return;
  auto m = m_model->ports[port].mapping;
  if(std::all_of(m.begin(), m.end(), [](int c) { return c == unrouted; }))
    return;
  std::fill(m.begin(), m.end(), unrouted);
  route({{port, std::move(m)}});
}

void RoutingMatrix::disconnectAll()
{
  Changes res;
  for(int p = 0; p < int(m_model->ports.size()); p++)
  {
    auto m = m_model->ports[p].mapping;
    if(std::all_of(m.begin(), m.end(), [](int c) { return c == unrouted; }))
      continue;
    std::fill(m.begin(), m.end(), unrouted);
    res.emplace_back(p, std::move(m));
  }
  route(std::move(res));
}

void RoutingMatrix::mousePressEvent(QMouseEvent* e)
{
  const auto idx = indexAt(e->position().toPoint());
  if(e->button() != Qt::LeftButton || !idx.isValid())
  {
    QTableView::mousePressEvent(e);
    return;
  }
  setCurrentIndex(idx);
  if(e->modifiers() & Qt::ControlModifier)
    connectDiagonally(idx.row(), idx.column(), e->modifiers() & Qt::ShiftModifier);
  else
    toggle(idx.row(), idx.column());
  e->accept();
}

void RoutingMatrix::mouseMoveEvent(QMouseEvent* e)
{
  setHovered(indexAt(e->position().toPoint()));
  QTableView::mouseMoveEvent(e);
}

void RoutingMatrix::leaveEvent(QEvent* e)
{
  setHovered({});
  QTableView::leaveEvent(e);
}

void RoutingMatrix::setHovered(const QModelIndex& idx)
{
  if(m_model->setHovered(
         idx.isValid() ? idx.row() : -1, idx.isValid() ? idx.column() : -1))
    viewport()->update();
}

void RoutingMatrix::keyPressEvent(QKeyEvent* e)
{
  const auto idx = currentIndex();
  switch(e->key())
  {
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
      if(idx.isValid())
      {
        if(e->modifiers() & Qt::ControlModifier)
          connectDiagonally(idx.row(), idx.column(), e->modifiers() & Qt::ShiftModifier);
        else
          toggle(idx.row(), idx.column());
        e->accept();
        return;
      }
      break;
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
      if(idx.isValid())
      {
        disconnectRow(idx.row());
        e->accept();
        return;
      }
      break;
  }
  QTableView::keyPressEvent(e);
}

void RoutingMatrix::contextMenuEvent(QContextMenuEvent* e)
{
  QMenu menu;
  const auto idx = indexAt(viewport()->mapFromGlobal(e->globalPos()));
  bool any_routed = false;
  for(int r = 0; r < int(m_model->rows.size()); r++)
    any_routed |= m_model->routeOf(r) != unrouted;

  if(idx.isValid())
  {
    const int row = idx.row();
    const int col = idx.column();
    const auto [p, ch] = m_model->rows[row];
    const bool on = m_model->routeOf(row) == col;

    auto toggle_act = menu.addAction(
        on ? QObject::tr("Disconnect %1").arg(m_model->routeText(row, col))
           : QObject::tr("Connect %1").arg(m_model->routeText(row, col)));
    QObject::connect(
        toggle_act, &QAction::triggered, this, [this, row, col] { toggle(row, col); });

    menu.addSeparator();
    auto diag_port = menu.addAction(
        QObject::tr("Connect diagonally to the end of the port") + '\t' + ctrlName()
        + QObject::tr("Click"));
    QObject::connect(diag_port, &QAction::triggered, this, [this, row, col] {
      connectDiagonally(row, col, false);
    });
    auto diag_all = menu.addAction(
        QObject::tr("Connect diagonally to the end of the list") + '\t' + ctrlName()
        + shiftName() + QObject::tr("Click"));
    QObject::connect(diag_all, &QAction::triggered, this, [this, row, col] {
      connectDiagonally(row, col, true);
    });

    menu.addSeparator();
    auto off_row = menu.addAction(
        QObject::tr("Disconnect %1").arg(m_model->rowName(row)) + '\t'
        + QKeySequence(Qt::Key_Delete).toString(QKeySequence::NativeText));
    off_row->setEnabled(m_model->routeOf(row) != unrouted);
    QObject::connect(
        off_row, &QAction::triggered, this, [this, row] { disconnectRow(row); });

    auto off_port = menu.addAction(
        QObject::tr("Disconnect every channel of %1").arg(m_model->ports[p].name));
    QObject::connect(
        off_port, &QAction::triggered, this, [this, p = p] { disconnectPort(p); });
  }

  auto off_all = menu.addAction(QObject::tr("Disconnect everything"));
  off_all->setEnabled(any_routed);
  QObject::connect(off_all, &QAction::triggered, this, [this] { disconnectAll(); });

  menu.exec(e->globalPos());
  e->accept();
}
}
