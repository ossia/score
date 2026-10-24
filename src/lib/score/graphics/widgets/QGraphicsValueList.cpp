#include "QGraphicsValueList.hpp"

#include <score/graphics/widgets/QGraphicsCombo.hpp>
#include <score/graphics/widgets/QGraphicsSpinbox.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Debug.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QRegularExpression>
#include <QTimer>

#include <wobjectimpl.h>

#include <cmath>
#include <limits>
#include <optional>

W_OBJECT_IMPL(score::QGraphicsValueList);

namespace score
{
namespace
{
constexpr qreal cell_height = 20.;
constexpr qreal row_pitch = 22.;
constexpr qreal number_width = 40.;
constexpr qreal gap = 2.;
constexpr qreal button_width = 14.;
constexpr qreal index_width = 16.;

QString columnToText(const ValueListColumn& c, double v)
{
  if(!c.choices.isEmpty())
  {
    const int i = std::clamp(int(v), 0, int(c.choices.size() - 1));
    const QString& name = c.choices[i];
    // Names with blanks would split into several tokens
    return name.contains(' ') ? QString::number(i) : name;
  }
  if(c.integral)
    return QString::number(std::lround(v));
  return QString::number(v, 'g', 6);
}

std::optional<double> columnFromText(const ValueListColumn& c, const QString& tok)
{
  bool ok = false;
  const double d = tok.toDouble(&ok);
  if(ok)
    return d;
  for(int i = 0; i < c.choices.size(); i++)
  {
    QString name = c.choices[i];
    name.remove(' ');
    if(name.compare(tok, Qt::CaseInsensitive) == 0)
      return i;
  }
  return std::nullopt;
}
}

QGraphicsValueList::QGraphicsValueList(QGraphicsItem* parent)
    : QGraphicsItem{parent}
{
  setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
  rebuild();
}

QGraphicsValueList::~QGraphicsValueList() = default;

void QGraphicsValueList::setColumns(std::vector<ValueListColumn> cols)
{
  if(cols.empty())
    cols.push_back(ValueListColumn{});
  m_columns = std::move(cols);
  for(auto& r : m_rows)
    r = clampRow(std::move(r));
  rebuild();
}

void QGraphicsValueList::setCountRange(int min, int max)
{
  m_minCount = std::max(0, min);
  m_maxCount = max;
  rebuild();
}

void QGraphicsValueList::setItemsPerLine(int n)
{
  m_itemsPerLine = std::max(1, n);
  rebuild();
}

int QGraphicsValueList::maxCount() const noexcept
{
  return m_maxCount < 0 ? std::numeric_limits<int>::max() : m_maxCount;
}

QGraphicsValueList::Row QGraphicsValueList::defaultRow() const
{
  Row r;
  for(auto& c : m_columns)
    r.push_back(c.init);
  return clampRow(std::move(r));
}

QGraphicsValueList::Row QGraphicsValueList::clampRow(Row r) const
{
  r.resize(m_columns.size());
  for(std::size_t i = 0; i < m_columns.size(); i++)
  {
    const auto& c = m_columns[i];
    double v = r[i];
    if(!std::isfinite(v))
      v = c.init;
    if(!c.choices.isEmpty())
      v = std::clamp(std::round(v), 0., double(c.choices.size() - 1));
    else
    {
      if(c.integral)
        v = std::round(v);
      v = std::clamp(v, std::min(c.min, c.max), std::max(c.min, c.max));
    }
    r[i] = v;
  }
  return r;
}

void QGraphicsValueList::setValue(const ossia::value& v)
{
  std::vector<Row> rows;
  auto to_row = [&](const ossia::value& e) {
    Row r;
    if(auto l = e.target<std::vector<ossia::value>>())
      for(auto& x : *l)
        r.push_back(ossia::convert<double>(x));
    else if(auto v2 = e.target<ossia::vec2f>())
      r.assign(v2->begin(), v2->end());
    else if(auto v3 = e.target<ossia::vec3f>())
      r.assign(v3->begin(), v3->end());
    else if(auto v4 = e.target<ossia::vec4f>())
      r.assign(v4->begin(), v4->end());
    else
      r.push_back(ossia::convert<double>(e));
    return clampRow(std::move(r));
  };

  if(auto l = v.target<std::vector<ossia::value>>())
  {
    for(auto& e : *l)
      rows.push_back(to_row(e));
  }
  else if(v.valid() && scalar())
  {
    rows.push_back(to_row(v));
  }

  if(int(rows.size()) > maxCount())
    rows.resize(maxCount());

  // Against the cells shown: a structural edit changes m_rows before its
  // deferred rebuild() resizes them
  if(rows.size() == m_cells.size())
  {
    m_rows = std::move(rows);
    updateCells();
  }
  else
  {
    m_rows = std::move(rows);
    rebuild();
  }
}

ossia::value QGraphicsValueList::value() const
{
  auto cell = [this](std::size_t c, double v) -> ossia::value {
    const auto& col = m_columns[c];
    if(col.integral || !col.choices.isEmpty())
      return int(std::lround(v));
    return float(v);
  };

  std::vector<ossia::value> res;
  res.reserve(m_rows.size());
  for(auto& r : m_rows)
  {
    if(scalar())
    {
      res.push_back(cell(0, r[0]));
    }
    else
    {
      std::vector<ossia::value> sub;
      sub.reserve(r.size());
      for(std::size_t c = 0; c < r.size(); c++)
        sub.push_back(cell(c, r[c]));
      res.push_back(std::move(sub));
    }
  }
  return res;
}

QString QGraphicsValueList::toText() const
{
  QStringList rows;
  for(auto& r : m_rows)
  {
    QStringList cols;
    for(std::size_t c = 0; c < r.size(); c++)
      cols.push_back(columnToText(m_columns[c], r[c]));
    rows.push_back(cols.join(' '));
  }
  return rows.join(scalar() ? QStringLiteral(" ") : QStringLiteral(", "));
}

bool QGraphicsValueList::setText(const QString& text)
{
  static const QRegularExpression row_sep{QStringLiteral("[,;\\n]")};
  static const QRegularExpression blank{QStringLiteral("\\s+")};

  std::vector<Row> rows;
  if(scalar())
  {
    // Commas and blanks both separate numbers here
    for(const auto& tok :
        QString{text}.replace(row_sep, " ").split(blank, Qt::SkipEmptyParts))
    {
      auto v = columnFromText(m_columns[0], tok);
      if(!v)
        return false;
      rows.push_back(clampRow({*v}));
    }
  }
  else
  {
    for(const auto& line : text.split(row_sep, Qt::SkipEmptyParts))
    {
      const auto toks = line.split(blank, Qt::SkipEmptyParts);
      if(toks.isEmpty())
        continue;
      // Missing trailing columns take their default
      Row r = defaultRow();
      for(int c = 0; c < toks.size() && c < int(m_columns.size()); c++)
      {
        auto v = columnFromText(m_columns[c], toks[c]);
        if(!v)
          return false;
        r[c] = *v;
      }
      rows.push_back(clampRow(std::move(r)));
    }
  }

  if(int(rows.size()) < m_minCount || int(rows.size()) > maxCount())
    return false;

  m_rows = std::move(rows);
  structuralEdit();
  return true;
}

void QGraphicsValueList::editAsText()
{
  QWidget* parent{};
  if(auto s = scene(); s && !s->views().isEmpty())
    parent = s->views().front();

  QString text = toText();
  QString hint = scalar() ? QObject::tr("Numbers separated by spaces")
                          : QObject::tr("One element per comma, columns separated by spaces");
  if(!scalar())
  {
    QStringList names;
    for(auto& c : m_columns)
      names.push_back(c.name);
    hint += QStringLiteral(": ") + names.join(' ');
  }

  for(;;)
  {
    bool ok = false;
    text = QInputDialog::getText(
        parent, QObject::tr("Edit list"), hint, QLineEdit::Normal, text, &ok);
    if(!ok || setText(text))
      return;
    hint = QObject::tr("Could not read that list, or it has too many or too few elements.");
  }
}

qreal QGraphicsValueList::columnWidth(int col) const noexcept
{
  const auto& c = m_columns[col];
  if(c.choices.isEmpty())
    return number_width;

  auto& skin = score::Skin::instance();
  QFontMetricsF metrics{skin.Medium10Pt};
  qreal w = 60.;
  for(auto& s : c.choices)
    w = std::max(w, metrics.horizontalAdvance(s) + 16. + QGraphicsCombo::stepperWidth);
  return std::min(w, 140.);
}

qreal QGraphicsValueList::headerHeight() const noexcept
{
  if(scalar())
    return 0.;
  for(auto& c : m_columns)
    if(!c.name.isEmpty())
      return 12.;
  return 0.;
}

QRectF QGraphicsValueList::cellRect(int row, int col) const noexcept
{
  if(scalar())
  {
    const qreal pitch = columnWidth(0) + gap;
    const int line = row / m_itemsPerLine;
    const int pos = row % m_itemsPerLine;
    return QRectF{pos * pitch, line * row_pitch, columnWidth(0), cell_height};
  }

  qreal x = index_width;
  for(int c = 0; c < col; c++)
    x += columnWidth(c) + gap;
  return QRectF{x, headerHeight() + row * row_pitch, columnWidth(col), cell_height};
}

QRectF QGraphicsValueList::removeRect(int row) const noexcept
{
  if(scalar())
    return {};
  const QRectF last = cellRect(row, int(m_columns.size()) - 1);
  return QRectF{last.right() + gap, last.top(), button_width, cell_height};
}

QRectF QGraphicsValueList::addRect() const noexcept
{
  if(scalar())
  {
    // The two buttons take the slot after the last cell
    const int n = int(m_rows.size());
    const qreal pitch = columnWidth(0) + gap;
    const int line = n / m_itemsPerLine;
    const int pos = n % m_itemsPerLine;
    return QRectF{pos * pitch, line * row_pitch, button_width, cell_height};
  }
  return QRectF{0., headerHeight() + m_rows.size() * row_pitch, button_width, cell_height};
}

QRectF QGraphicsValueList::popRect() const noexcept
{
  if(!scalar())
    return {};
  return addRect().translated(button_width + gap, 0.);
}

int QGraphicsValueList::rowAt(QPointF p) const noexcept
{
  for(int r = 0; r < int(m_rows.size()); r++)
  {
    QRectF row = cellRect(r, 0);
    if(!scalar())
      row = row.united(removeRect(r)).adjusted(-index_width, 0, 0, 0);
    if(row.adjusted(-gap / 2., -gap / 2., gap / 2., gap / 2.).contains(p))
      return r;
  }
  return -1;
}

void QGraphicsValueList::rebuild()
{
  for(auto& row : m_cells)
    for(auto* c : row)
      delete c;
  m_cells.clear();

  for(int r = 0; r < int(m_rows.size()); r++)
  {
    std::vector<QGraphicsItem*> cells;
    for(int c = 0; c < int(m_columns.size()); c++)
    {
      const auto& col = m_columns[c];
      const QRectF rect = cellRect(r, c);
      QGraphicsItem* item{};
      auto on_moved = [this, r, c](double v) {
        if(r >= int(m_rows.size()))
          return;
        m_rows[r][c] = v;
        moving = true;
        sliderMoved();
      };
      auto on_released = [this] {
        moving = false;
        sliderReleased();
      };

      if(!col.choices.isEmpty())
      {
        auto combo = new QGraphicsCombo{col.choices, this};
        combo->setRect(QRectF{0., 0., rect.width(), cell_height});
        combo->setValue(int(m_rows[r][c]));
        connect(combo, &QGraphicsCombo::sliderMoved, this, [combo, on_moved] {
          on_moved(combo->value());
        });
        connect(combo, &QGraphicsCombo::sliderReleased, this, on_released);
        item = combo;
      }
      else if(col.integral)
      {
        auto sb = new QGraphicsIntSpinbox{this};
        sb->setRange(col.min, col.max, col.init);
        sb->setValue(m_rows[r][c]);
        connect(sb, &QGraphicsIntSpinbox::sliderMoved, this, [sb, on_moved] {
          on_moved(sb->value());
        });
        connect(sb, &QGraphicsIntSpinbox::sliderReleased, this, on_released);
        item = sb;
      }
      else
      {
        auto sb = new QGraphicsSpinbox{this};
        sb->setRange(col.min, col.max, col.init);
        sb->setValue(sb->unmap(m_rows[r][c]));
        connect(sb, &QGraphicsSpinbox::sliderMoved, this, [sb, on_moved] {
          on_moved(sb->map(sb->value()));
        });
        connect(sb, &QGraphicsSpinbox::sliderReleased, this, on_released);
        item = sb;
      }
      item->setPos(rect.topLeft());
      if(!col.name.isEmpty())
        item->setToolTip(col.name);
      cells.push_back(item);
    }
    m_cells.push_back(std::move(cells));
  }

  // Size: every cell and button, the header, and room for a full line
  QRectF r = addRect().united(popRect());
  for(int i = 0; i < int(m_rows.size()); i++)
    for(int c = 0; c < int(m_columns.size()); c++)
      r = r.united(cellRect(i, c));
  if(!scalar())
  {
    for(int i = 0; i < int(m_rows.size()); i++)
      r = r.united(removeRect(i));
    r = r.united(QRectF{0., 0., cellRect(0, int(m_columns.size()) - 1).right()
                                    + gap + button_width, headerHeight()});
  }
  else
  {
    const qreal line = std::min<int>(m_itemsPerLine, std::max(1, int(m_rows.size())));
    r = r.united(QRectF{0., 0., line * (columnWidth(0) + gap) - gap, cell_height});
  }
  r.setTopLeft({0., 0.});

  if(r != m_rect)
  {
    prepareGeometryChange();
    m_rect = r;
    sizeChanged(m_rect.size());
  }
  update();
}

void QGraphicsValueList::updateCells()
{
  for(int r = 0; r < int(m_cells.size()); r++)
  {
    for(int c = 0; c < int(m_cells[r].size()); c++)
    {
      const double v = m_rows[r][c];
      auto* item = m_cells[r][c];
      if(auto combo = qgraphicsitem_cast<QGraphicsCombo*>(item))
        combo->setValue(int(v));
      else if(auto isb = qgraphicsitem_cast<QGraphicsIntSpinbox*>(item))
        isb->setValue(v);
      else if(auto sb = qgraphicsitem_cast<QGraphicsSpinbox*>(item))
        sb->setValue(sb->unmap(v));
    }
  }
}

void QGraphicsValueList::structuralEdit()
{
  // Deferred: the press that asked for it may come from a cell rebuild() deletes
  QTimer::singleShot(0, this, [this] {
    rebuild();
    edited();
  });
}

void QGraphicsValueList::insertRow(int at, Row r)
{
  if(int(m_rows.size()) >= maxCount())
    return;
  at = std::clamp(at, 0, int(m_rows.size()));
  m_rows.insert(m_rows.begin() + at, clampRow(std::move(r)));
  structuralEdit();
}

void QGraphicsValueList::removeRow(int row)
{
  if(row < 0 || row >= int(m_rows.size()) || int(m_rows.size()) <= m_minCount)
    return;
  m_rows.erase(m_rows.begin() + row);
  structuralEdit();
}

void QGraphicsValueList::moveRow(int row, int dir)
{
  const int other = row + dir;
  if(row < 0 || other < 0 || row >= int(m_rows.size()) || other >= int(m_rows.size()))
    return;
  std::swap(m_rows[row], m_rows[other]);
  structuralEdit();
}

void QGraphicsValueList::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_pressed = 0;
  if(event->button() == Qt::LeftButton)
  {
    const QPointF p = event->pos();
    if(addRect().contains(p))
      m_pressed = 1;
    else if(popRect().contains(p))
      m_pressed = 2;
    else
      for(int r = 0; r < int(m_rows.size()); r++)
        if(removeRect(r).contains(p))
          m_pressed = 3 + r;
  }
  update();
  event->accept();
}

void QGraphicsValueList::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  const int pressed = std::exchange(m_pressed, 0);
  update();
  event->accept();

  const QPointF p = event->pos();
  if(pressed == 1 && addRect().contains(p))
    insertRow(int(m_rows.size()), m_rows.empty() ? defaultRow() : m_rows.back());
  else if(pressed == 2 && popRect().contains(p))
    removeRow(int(m_rows.size()) - 1);
  else if(pressed >= 3 && removeRect(pressed - 3).contains(p))
    removeRow(pressed - 3);
}

void QGraphicsValueList::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  // A quick second click on a button is a second click, not the text editor
  const QPointF p = event->pos();
  if(addRect().contains(p) || popRect().contains(p))
  {
    mousePressEvent(event);
    return;
  }
  for(int r = 0; r < int(m_rows.size()); r++)
  {
    if(removeRect(r).contains(p))
    {
      mousePressEvent(event);
      return;
    }
  }
  event->accept();
  editAsText();
}

void QGraphicsValueList::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  event->accept();
  const int row = rowAt(event->pos());
  const int n = int(m_rows.size());
  const bool can_add = n < maxCount();
  const bool can_remove = n > m_minCount;
  const QString earlier = scalar() ? QObject::tr("Move left") : QObject::tr("Move up");
  const QString later = scalar() ? QObject::tr("Move right") : QObject::tr("Move down");

  auto menu = new QMenu;
  menu->setAttribute(Qt::WA_DeleteOnClose);
  if(row >= 0)
  {
    const Row current = m_rows[row];
    menu->addAction(QObject::tr("Insert before"), this, [this, row, current] {
      insertRow(row, current);
    })->setEnabled(can_add);
    menu->addAction(QObject::tr("Duplicate"), this, [this, row, current] {
      insertRow(row + 1, current);
    })->setEnabled(can_add);
    menu->addAction(earlier, this, [this, row] { moveRow(row, -1); })
        ->setEnabled(row > 0);
    menu->addAction(later, this, [this, row] { moveRow(row, +1); })
        ->setEnabled(row + 1 < n);
    menu->addAction(QObject::tr("Remove"), this, [this, row] { removeRow(row); })
        ->setEnabled(can_remove);
    menu->addSeparator();
  }
  menu->addAction(QObject::tr("Add"), this, [this] {
    insertRow(int(m_rows.size()), m_rows.empty() ? defaultRow() : m_rows.back());
  })->setEnabled(can_add);
  menu->addAction(QObject::tr("Clear"), this, [this] {
    m_rows.resize(m_minCount, defaultRow());
    structuralEdit();
  })->setEnabled(n > m_minCount);
  menu->addSeparator();
  menu->addAction(QObject::tr("Edit as text"), this, [this] { editAsText(); });
  menu->popup(event->screenPos());
}

QRectF QGraphicsValueList::boundingRect() const
{
  return m_rect;
}

void QGraphicsValueList::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& skin = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);

  // Column names, and the row numbers that the context menu refers to
  if(!scalar())
  {
    painter->setFont(skin.Medium7Pt);
    painter->setPen(skin.Gray.main.pen1);
    if(headerHeight() > 0.)
    {
      for(int c = 0; c < int(m_columns.size()); c++)
      {
        const QRectF cell = cellRect(0, c);
        painter->drawText(
            QRectF{cell.left(), 0., cell.width(), headerHeight()}, m_columns[c].name,
            QTextOption{Qt::AlignLeft | Qt::AlignVCenter});
      }
    }
    for(int r = 0; r < int(m_rows.size()); r++)
    {
      const QRectF cell = cellRect(r, 0);
      painter->drawText(
          QRectF{0., cell.top(), index_width - gap, cell_height},
          QString::number(r + 1), QTextOption{Qt::AlignRight | Qt::AlignVCenter});
    }
  }

  auto button = [&](const QRectF& rect, int id, bool enabled, auto&& glyph) {
    if(rect.isEmpty())
      return;
    painter->setPen(skin.NoPen);
    painter->setBrush(m_pressed == id ? skin.Base4.main.brush : skin.Emphasis2.main.brush);
    painter->drawRoundedRect(rect, 1, 1);
    QPen pen = (m_pressed == id ? skin.Emphasis2 : enabled ? skin.Base4 : skin.Gray)
                   .main.pen1;
    pen.setWidthF(1.5);
    painter->setPen(pen);
    glyph(rect.center());
  };
  const qreal arm = 3.;
  auto plus = [&](QPointF c) {
    painter->drawLine(QPointF{c.x() - arm, c.y()}, QPointF{c.x() + arm, c.y()});
    painter->drawLine(QPointF{c.x(), c.y() - arm}, QPointF{c.x(), c.y() + arm});
  };
  auto minus = [&](QPointF c) {
    painter->drawLine(QPointF{c.x() - arm, c.y()}, QPointF{c.x() + arm, c.y()});
  };
  auto cross = [&](QPointF c) {
    const qreal a = arm * 0.8;
    painter->drawLine(QPointF{c.x() - a, c.y() - a}, QPointF{c.x() + a, c.y() + a});
    painter->drawLine(QPointF{c.x() - a, c.y() + a}, QPointF{c.x() + a, c.y() - a});
  };

  const int n = int(m_rows.size());
  button(addRect(), 1, n < maxCount(), plus);
  button(popRect(), 2, n > m_minCount, minus);
  for(int r = 0; r < n; r++)
    button(removeRect(r), 3 + r, n > m_minCount, cross);

  painter->setRenderHint(QPainter::Antialiasing, false);
}
}
