#include "QGraphicsChannelMask.hpp"

#include <score/model/Skin.hpp>

#include <ossia/network/value/value_conversion.hpp>

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

W_OBJECT_IMPL(score::QGraphicsChannelMask);

namespace score
{
namespace
{
constexpr qreal mask_cell_w = 16.;
constexpr qreal mask_cell_h = 14.;
constexpr qreal mask_gap = 1.;
constexpr int per_row = 8;
}

QGraphicsChannelMask::QGraphicsChannelMask(QGraphicsItem* parent)
    : QGraphicsItem{parent}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
  m_on.fill(true);
}

QGraphicsChannelMask::~QGraphicsChannelMask() = default;

void QGraphicsChannelMask::setValue(const ossia::value& v)
{
  std::array<bool, 16> on{};
  if(auto l = v.target<std::vector<ossia::value>>())
    for(int i = 0; i < 16 && i < int(l->size()); i++)
      on[i] = ossia::convert<bool>((*l)[i]);
  m_on = on;
  update();
}

ossia::value QGraphicsChannelMask::value() const
{
  std::vector<ossia::value> res;
  res.reserve(16);
  for(bool b : m_on)
    res.push_back(b);
  return res;
}

QString QGraphicsChannelMask::toText() const
{
  // Runs of channels as ranges: "1-4, 10"
  QStringList parts;
  for(int i = 0; i < 16;)
  {
    if(!m_on[i])
    {
      i++;
      continue;
    }
    int j = i;
    while(j + 1 < 16 && m_on[j + 1])
      j++;
    parts.push_back(
        j == i ? QString::number(i + 1) : QStringLiteral("%1-%2").arg(i + 1).arg(j + 1));
    i = j + 1;
  }
  return parts.join(QStringLiteral(", "));
}

bool QGraphicsChannelMask::setText(const QString& text)
{
  static const QRegularExpression sep{QStringLiteral("[\\s,;]+")};
  static const QRegularExpression range{QStringLiteral("^(\\d+)(?:-(\\d+))?$")};
  std::array<bool, 16> on{};
  for(const auto& tok : text.split(sep, Qt::SkipEmptyParts))
  {
    const auto m = range.match(tok);
    if(!m.hasMatch())
      return false;
    const int a = m.captured(1).toInt();
    const int b = m.captured(2).isEmpty() ? a : m.captured(2).toInt();
    if(a < 1 || b > 16 || a > b)
      return false;
    for(int c = a; c <= b; c++)
      on[c - 1] = true;
  }
  set(on);
  return true;
}

void QGraphicsChannelMask::set(std::array<bool, 16> on)
{
  m_on = on;
  update();
  sliderMoved();
  sliderReleased();
}

QRectF QGraphicsChannelMask::cellRect(int channel) const noexcept
{
  const int row = channel / per_row;
  const int col = channel % per_row;
  return QRectF{col * (mask_cell_w + mask_gap), row * (mask_cell_h + mask_gap), mask_cell_w, mask_cell_h};
}

int QGraphicsChannelMask::channelAt(QPointF p) const noexcept
{
  for(int c = 0; c < 16; c++)
    if(cellRect(c).adjusted(-mask_gap, -mask_gap, mask_gap, mask_gap).contains(p))
      return c;
  return -1;
}

void QGraphicsChannelMask::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  if(event->button() != Qt::LeftButton)
    return;
  const int c = channelAt(event->pos());
  if(c < 0)
    return;
  m_grab = true;
  m_paint = !m_on[c];
  m_on[c] = m_paint;
  moving = true;
  update();
  sliderMoved();
}

void QGraphicsChannelMask::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  if(!m_grab)
    return;
  const int c = channelAt(event->pos());
  if(c >= 0 && m_on[c] != m_paint)
  {
    m_on[c] = m_paint;
    update();
    sliderMoved();
  }
}

void QGraphicsChannelMask::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
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
bool QGraphicsChannelMask::sceneEvent(QEvent* event)
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

void QGraphicsChannelMask::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  event->accept();
  auto menu = new QMenu;
  menu->setAttribute(Qt::WA_DeleteOnClose);
  menu->addAction(QObject::tr("All"), this, [this] {
    std::array<bool, 16> on;
    on.fill(true);
    set(on);
  });
  menu->addAction(QObject::tr("None"), this, [this] { set({}); });
  menu->addAction(QObject::tr("Invert"), this, [this] {
    auto on = m_on;
    for(auto& b : on)
      b = !b;
    set(on);
  });
  menu->addSeparator();
  menu->addAction(QObject::tr("Edit as text"), this, [this] {
    QWidget* parent{};
    if(auto s = scene(); s && !s->views().isEmpty())
      parent = s->views().front();
    // The dialog runs its own event loop, in which the item may be deleted
    QPointer<QGraphicsChannelMask> self{this};
    QString text = toText();
    QString hint = QObject::tr("Channels and ranges, e.g. 1-4, 10");
    for(;;)
    {
      bool ok = false;
      text = QInputDialog::getText(
          parent, QObject::tr("Edit channels"), hint, QLineEdit::Normal, text, &ok);
      if(!self || !ok || setText(text))
        return;
      hint = QObject::tr("Channels go from 1 to 16, e.g. 1-4, 10");
    }
  });
  menu->popup(event->screenPos());
}

QRectF QGraphicsChannelMask::boundingRect() const
{
  return QRectF{0., 0., per_row * (mask_cell_w + mask_gap), 2. * (mask_cell_h + mask_gap)};
}

void QGraphicsChannelMask::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& skin = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setFont(skin.Medium7Pt);
  const QColor lit{"#E0B01E"};
  for(int c = 0; c < 16; c++)
  {
    const QRectF r = cellRect(c);
    painter->setPen(skin.NoPen);
    painter->setBrush(m_on[c] ? QBrush{lit} : skin.Emphasis2.main.brush);
    painter->drawRoundedRect(r, 1, 1);
    painter->setPen(m_on[c] ? skin.Emphasis2.main.pen1 : skin.Gray.main.pen1);
    painter->drawText(r, QString::number(c + 1), QTextOption{Qt::AlignCenter});
  }
  painter->setRenderHint(QPainter::Antialiasing, false);
}
}
