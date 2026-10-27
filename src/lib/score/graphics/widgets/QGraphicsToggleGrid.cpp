#include "QGraphicsToggleGrid.hpp"

#include <score/model/Skin.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QFontMetricsF>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QRegularExpression>

#include <wobjectimpl.h>

W_OBJECT_IMPL(score::QGraphicsToggleGrid);

namespace score
{
namespace
{
constexpr qreal toggle_cell_h = 14.;
constexpr qreal toggle_gap = 1.;
constexpr qreal toggle_min_cell_w = 16.;
constexpr qreal toggle_text_margin = 6.;
}

QGraphicsToggleGrid::QGraphicsToggleGrid(QGraphicsItem* parent)
    : QGraphicsItem{parent}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

QGraphicsToggleGrid::~QGraphicsToggleGrid() = default;

void QGraphicsToggleGrid::setLabels(QStringList labels, int perRow)
{
  prepareGeometryChange();
  m_labels = std::move(labels);
  m_on.assign(m_labels.size(), false);
  m_perRow = std::max(1, perRow);

  const QFontMetricsF metrics{score::Skin::instance().Medium7Pt};
  m_cellWidth = toggle_min_cell_w;
  for(const auto& l : m_labels)
    m_cellWidth = std::max(m_cellWidth, metrics.horizontalAdvance(l) + toggle_text_margin);
  update();
}

void QGraphicsToggleGrid::setValue(const ossia::value& v)
{
  std::vector<bool> on(m_labels.size(), false);
  if(auto l = v.target<std::vector<ossia::value>>())
    for(std::size_t i = 0; i < on.size() && i < l->size(); i++)
      on[i] = ossia::convert<bool>((*l)[i]);
  m_on = std::move(on);
  update();
}

ossia::value QGraphicsToggleGrid::value() const
{
  std::vector<ossia::value> res;
  res.reserve(m_on.size());
  for(bool b : m_on)
    res.push_back(b);
  return res;
}

QString QGraphicsToggleGrid::toText() const
{
  QStringList parts;
  for(int i = 0; i < std::ssize(m_on); i++)
    if(m_on[i])
      parts.push_back(m_labels[i]);
  return parts.join(QStringLiteral(", "));
}

bool QGraphicsToggleGrid::setText(const QString& text)
{
  static const QRegularExpression sep{QStringLiteral("[,;]+")};
  std::vector<bool> on(m_labels.size(), false);
  for(const auto& tok : text.split(sep, Qt::SkipEmptyParts))
  {
    const QString name = tok.trimmed();
    if(name.isEmpty())
      continue;
    const auto it = std::find_if(m_labels.begin(), m_labels.end(), [&](const QString& l) {
      return l.compare(name, Qt::CaseInsensitive) == 0;
    });
    if(it == m_labels.end())
      return false;
    on[it - m_labels.begin()] = true;
  }
  set(std::move(on));
  return true;
}

QString QGraphicsToggleGrid::editTitle() const
{
  return QObject::tr("Edit");
}

QString QGraphicsToggleGrid::editHint() const
{
  return QObject::tr("Names separated by commas, e.g. %1")
      .arg(m_labels.mid(0, 2).join(QStringLiteral(", ")));
}

QString QGraphicsToggleGrid::editError() const
{
  return QObject::tr("Unknown name. The names are: %1")
      .arg(m_labels.join(QStringLiteral(", ")));
}

void QGraphicsToggleGrid::set(std::vector<bool> on)
{
  on.resize(m_labels.size(), false);
  m_on = std::move(on);
  update();
  sliderMoved();
  sliderReleased();
}

QRectF QGraphicsToggleGrid::cellRect(int cell) const noexcept
{
  const int row = cell / m_perRow;
  const int col = cell % m_perRow;
  return QRectF{
      col * (m_cellWidth + toggle_gap), row * (toggle_cell_h + toggle_gap), m_cellWidth, toggle_cell_h};
}

int QGraphicsToggleGrid::cellAt(QPointF p) const noexcept
{
  for(int c = 0; c < std::ssize(m_on); c++)
    if(cellRect(c).adjusted(-toggle_gap, -toggle_gap, toggle_gap, toggle_gap).contains(p))
      return c;
  return -1;
}

void QGraphicsToggleGrid::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  if(event->button() != Qt::LeftButton)
    return;
  const int c = cellAt(event->pos());
  if(c < 0)
    return;
  m_grab = true;
  m_paint = !m_on[c];
  m_on[c] = m_paint;
  moving = true;
  update();
  sliderMoved();
}

void QGraphicsToggleGrid::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  if(!m_grab)
    return;
  const int c = cellAt(event->pos());
  if(c >= 0 && m_on[c] != m_paint)
  {
    m_on[c] = m_paint;
    update();
    sliderMoved();
  }
}

void QGraphicsToggleGrid::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  if(!std::exchange(m_grab, false))
    return;
  moving = false;
  sliderReleased();
}

//! QEvent::UngrabMouse: the scene took the implicit grab away and there will
//! be no release to end the drag on. See DefaultGraphicsSliderImpl for what
//! goes wrong if the edit is left open.
bool QGraphicsToggleGrid::sceneEvent(QEvent* event)
{
  if(event->type() == QEvent::UngrabMouse)
  {
    if(std::exchange(m_grab, false))
    {
      moving = false;
      sliderReleased();
    }
  }
  return QGraphicsItem::sceneEvent(event);
}

void QGraphicsToggleGrid::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  event->accept();
  auto menu = new QMenu;
  menu->setAttribute(Qt::WA_DeleteOnClose);
  menu->addAction(QObject::tr("All"), this, [this] {
    set(std::vector<bool>(m_labels.size(), true));
  });
  menu->addAction(QObject::tr("None"), this, [this] { set({}); });
  menu->addAction(QObject::tr("Invert"), this, [this] {
    auto on = m_on;
    on.flip();
    set(std::move(on));
  });
  menu->addSeparator();
  menu->addAction(QObject::tr("Edit as text"), this, [this] {
    QWidget* parent{};
    if(auto s = scene(); s && !s->views().isEmpty())
      parent = s->views().front();
    // The dialog runs its own event loop, in which the item may be deleted
    QPointer<QGraphicsToggleGrid> self{this};
    QString text = toText();
    QString hint = editHint();
    const QString title = editTitle();
    for(;;)
    {
      bool ok = false;
      text = QInputDialog::getText(parent, title, hint, QLineEdit::Normal, text, &ok);
      if(!self || !ok || setText(text))
        return;
      hint = editError();
    }
  });
  menu->popup(event->screenPos());
}

QRectF QGraphicsToggleGrid::boundingRect() const
{
  const int n = std::max<int>(1, m_on.size());
  const int cols = std::min(n, m_perRow);
  const int rows = (n + m_perRow - 1) / m_perRow;
  return QRectF{0., 0., cols * (m_cellWidth + toggle_gap), rows * (toggle_cell_h + toggle_gap)};
}

void QGraphicsToggleGrid::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& skin = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setFont(skin.Medium7Pt);
  const QColor lit{"#E0B01E"};
  for(int c = 0; c < std::ssize(m_on); c++)
  {
    const QRectF r = cellRect(c);
    painter->setPen(skin.NoPen);
    painter->setBrush(m_on[c] ? QBrush{lit} : skin.Emphasis2.main.brush);
    painter->drawRoundedRect(r, 1, 1);
    painter->setPen(m_on[c] ? skin.Emphasis2.main.pen1 : skin.Gray.main.pen1);
    painter->drawText(r, m_labels[c], QTextOption{Qt::AlignCenter});
  }
  painter->setRenderHint(QPainter::Antialiasing, false);
}
}
