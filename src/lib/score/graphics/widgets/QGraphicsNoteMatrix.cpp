#include "QGraphicsNoteMatrix.hpp"

#include <score/model/Skin.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QRegularExpression>

#include <wobjectimpl.h>

W_OBJECT_IMPL(score::QGraphicsNoteMatrix);

namespace score
{
namespace
{
constexpr qreal cell_w = 14.;
constexpr qreal cell_h = 10.;
constexpr qreal label_w = 18.;
constexpr qreal header_h = 11.;

// Degrees from the root, in the spelling of the major scale
constexpr const char* degree_names[13]
    = {"1", "b2", "2", "b3", "3", "4", "#4", "5", "b6", "6", "b7", "7", "8"};

constexpr bool altered(int degree) noexcept
{
  switch(degree % 12)
  {
    case 1:
    case 3:
    case 6:
    case 8:
    case 10:
      return true;
    default:
      return false;
  }
}
}

QGraphicsNoteMatrix::QGraphicsNoteMatrix(QGraphicsItem* parent)
    : QGraphicsItem{parent}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

QGraphicsNoteMatrix::~QGraphicsNoteMatrix() = default;

void QGraphicsNoteMatrix::setValue(const ossia::value& v)
{
  std::array<int, columns> map;
  for(int i = 0; i < columns; i++)
    map[i] = i;
  if(auto l = v.target<std::vector<ossia::value>>())
  {
    for(int i = 0; i < columns && i < int(l->size()); i++)
    {
      const int d = ossia::convert<int>((*l)[i]);
      map[i] = (d < 0 || d >= rows) ? off : d;
    }
  }
  m_map = map;
  update();
}

ossia::value QGraphicsNoteMatrix::value() const
{
  std::vector<ossia::value> res;
  res.reserve(columns);
  for(int d : m_map)
    res.push_back(d);
  return res;
}

QString QGraphicsNoteMatrix::toText() const
{
  QStringList res;
  for(int d : m_map)
    res.push_back(d == off ? QStringLiteral("x") : QString::number(d));
  return res.join(' ');
}

bool QGraphicsNoteMatrix::setText(const QString& text)
{
  static const QRegularExpression sep{QStringLiteral("[\\s,;]+")};
  const auto toks = text.split(sep, Qt::SkipEmptyParts);
  if(toks.size() != columns)
    return false;

  std::array<int, columns> map;
  for(int i = 0; i < columns; i++)
  {
    const QString& t = toks[i];
    if(t.compare(QStringLiteral("x"), Qt::CaseInsensitive) == 0 || t == QStringLiteral("-"))
    {
      map[i] = off;
      continue;
    }
    bool ok = false;
    const int d = t.toInt(&ok);
    if(!ok || d < 0 || d >= rows)
      return false;
    map[i] = d;
  }
  set(map);
  return true;
}

void QGraphicsNoteMatrix::set(std::array<int, columns> map)
{
  m_map = map;
  update();
  sliderMoved();
  sliderReleased();
}

QRectF QGraphicsNoteMatrix::cellRect(int col, int row) const noexcept
{
  // Row 0 is at the bottom
  return QRectF{
      label_w + col * cell_w, header_h + (rows - 1 - row) * cell_h, cell_w, cell_h};
}

std::pair<int, int> QGraphicsNoteMatrix::cellAt(QPointF p) const noexcept
{
  const int col = int(std::floor((p.x() - label_w) / cell_w));
  const int row = rows - 1 - int(std::floor((p.y() - header_h) / cell_h));
  if(col < 0 || col >= columns || row < 0 || row >= rows)
    return {-1, -1};
  return {col, row};
}

void QGraphicsNoteMatrix::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  if(event->button() != Qt::LeftButton)
    return;
  const auto [col, row] = cellAt(event->pos());
  if(col < 0)
    return;
  m_grabbed = col;
  m_dragged = false;
  m_pressedLit = m_map[col] == row;
  if(!m_pressedLit)
  {
    m_map[col] = row;
    m_dragged = true;
    moving = true;
    sliderMoved();
  }
  update();
}

void QGraphicsNoteMatrix::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  if(m_grabbed < 0)
    return;
  const qreal y = event->pos().y() - header_h;
  const int row = std::clamp(rows - 1 - int(std::floor(y / cell_h)), 0, rows - 1);
  if(row != m_map[m_grabbed])
  {
    m_map[m_grabbed] = row;
    m_dragged = true;
    moving = true;
    update();
    sliderMoved();
  }
}

void QGraphicsNoteMatrix::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  const int col = std::exchange(m_grabbed, -1);
  if(col < 0)
    return;
  if(m_pressedLit && !m_dragged)
  {
    m_map[col] = off;
    moving = true;
    sliderMoved();
  }
  update();
  if(moving)
  {
    moving = false;
    sliderReleased();
  }
}

void QGraphicsNoteMatrix::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  event->accept();
  const auto [col, row] = cellAt(event->pos());

  auto menu = new QMenu;
  menu->setAttribute(Qt::WA_DeleteOnClose);
  if(col >= 0)
  {
    const bool muted = m_map[col] == off;
    menu->addAction(
        muted ? QObject::tr("Unmute %1").arg(degree_names[col])
              : QObject::tr("Mute %1").arg(degree_names[col]),
        this, [this, col, muted] {
      auto map = m_map;
      map[col] = muted ? col : off;
      set(map);
    });
    menu->addSeparator();
  }
  menu->addAction(QObject::tr("Identity"), this, [this] {
    std::array<int, columns> map;
    for(int i = 0; i < columns; i++)
      map[i] = i;
    set(map);
  });
  menu->addAction(QObject::tr("Invert"), this, [this] {
    // The descending diagonal: the root stays, the other degrees mirror
    // around it within the octave
    std::array<int, columns> map;
    map[0] = 0;
    for(int i = 1; i < columns; i++)
      map[i] = 12 - i;
    set(map);
  });
  menu->addAction(QObject::tr("Unmute all"), this, [this] {
    auto map = m_map;
    for(int i = 0; i < columns; i++)
      if(map[i] == off)
        map[i] = i;
    set(map);
  });
  menu->addSeparator();
  menu->addAction(QObject::tr("Edit as text"), this, [this] {
    QWidget* parent{};
    if(auto s = scene(); s && !s->views().isEmpty())
      parent = s->views().front();
    QString text = toText();
    QString hint = QObject::tr(
        "The output degree, 0 to 12, of each of the 12 degrees; x mutes one");
    for(;;)
    {
      bool ok = false;
      text = QInputDialog::getText(
          parent, QObject::tr("Edit map"), hint, QLineEdit::Normal, text, &ok);
      if(!ok || setText(text))
        return;
      hint = QObject::tr("12 values are needed, each 0 to 12 or x");
    }
  });
  menu->popup(event->screenPos());
}

QRectF QGraphicsNoteMatrix::boundingRect() const
{
  return QRectF{0., 0., label_w + columns * cell_w + 1., header_h + rows * cell_h + 1.};
}

void QGraphicsNoteMatrix::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& skin = score::Skin::instance();
  const QRectF grid{label_w, header_h, columns * cell_w, rows * cell_h};

  // Altered degrees darker, as black keys
  painter->setPen(skin.NoPen);
  painter->setBrush(skin.Emphasis2.main.brush);
  painter->drawRect(grid);
  QColor dark = skin.Emphasis2.main.brush.color().darker(140);
  for(int c = 0; c < columns; c++)
    if(altered(c))
      painter->fillRect(
          QRectF{label_w + c * cell_w, header_h, cell_w, rows * cell_h}, dark);
  for(int r = 0; r < rows; r++)
    if(altered(r))
      painter->fillRect(
          QRectF{label_w, header_h + (rows - 1 - r) * cell_h, columns * cell_w, cell_h},
          dark);

  // Grid
  QPen line = skin.Gray.main.pen1;
  line.setColor(skin.Emphasis2.main.brush.color().darker(180));
  painter->setPen(line);
  for(int c = 0; c <= columns; c++)
    painter->drawLine(
        QPointF{label_w + c * cell_w, header_h}, QPointF{label_w + c * cell_w, grid.bottom()});
  for(int r = 0; r <= rows; r++)
    painter->drawLine(
        QPointF{label_w, header_h + r * cell_h}, QPointF{grid.right(), header_h + r * cell_h});

  // The identity, for reference
  for(int c = 0; c < columns; c++)
  {
    const QRectF cell = cellRect(c, c).adjusted(5., 3.5, -5., -3.5);
    painter->fillRect(cell, line.color().lighter(160));
  }

  // The map
  const QColor lit{"#E0B01E"};
  for(int c = 0; c < columns; c++)
  {
    if(m_map[c] == off)
    {
      // A muted column: a cross in its header
      QPen p{skin.Gray.main.pen1};
      p.setWidthF(1.2);
      painter->setPen(p);
      const QRectF h{label_w + c * cell_w, 0., cell_w, header_h};
      const QPointF m = h.center();
      painter->drawLine(m + QPointF{-2.5, -2.5}, m + QPointF{2.5, 2.5});
      painter->drawLine(m + QPointF{-2.5, 2.5}, m + QPointF{2.5, -2.5});
      continue;
    }
    painter->fillRect(cellRect(c, m_map[c]).adjusted(1., 1., -1., -1.), lit);
  }

  // Degree names: inputs above, outputs on the left
  painter->setFont(skin.Medium7Pt);
  painter->setPen(skin.Gray.main.pen1);
  for(int c = 0; c < columns; c++)
  {
    if(m_map[c] == off)
      continue;
    painter->drawText(
        QRectF{label_w + c * cell_w, 0., cell_w, header_h}, degree_names[c],
        QTextOption{Qt::AlignCenter});
  }
  for(int r = 0; r < rows; r++)
  {
    if(altered(r))
      continue;
    painter->drawText(
        QRectF{0., header_h + (rows - 1 - r) * cell_h - 1., label_w - 3., cell_h + 2.},
        degree_names[r], QTextOption{Qt::AlignRight | Qt::AlignVCenter});
  }
}
}
