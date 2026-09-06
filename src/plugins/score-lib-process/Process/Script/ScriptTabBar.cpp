#include "ScriptTabBar.hpp"

#include <score/model/Skin.hpp>

#include <QCursor>
#include <QPaintEvent>
#include <QPainter>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Process::ScriptTabBar)
W_OBJECT_IMPL(Process::ScriptTabWidget)

namespace Process
{
static constexpr int flatTabPadding = 10;
static constexpr int flatTabHeight = 24;
static constexpr int flatUnderline = 2;

void ScriptTabBar::setFlat(bool flat)
{
  if(m_flat == flat)
    return;
  m_flat = flat;
  setAutoFillBackground(!flat);
  setDrawBase(!flat);
  updateGeometry();
  update();
}

QSize ScriptTabBar::tabSizeHint(int index) const
{
  if(!m_flat)
    return QTabBar::tabSizeHint(index);

  const int w = fontMetrics().horizontalAdvance(tabText(index)) + 2 * flatTabPadding;
  return {w, flatTabHeight};
}

void ScriptTabBar::paintEvent(QPaintEvent* ev)
{
  if(!m_flat)
  {
    QTabBar::paintEvent(ev);
    return;
  }

  auto& skin = score::Skin::instance();
  QPainter p{this};
  p.setFont(font());

  const QPoint cursor = mapFromGlobal(QCursor::pos());
  for(int i = 0; i < count(); i++)
  {
    const QRect r = tabRect(i);
    if(!r.intersects(ev->rect()))
      continue;

    const bool selected = i == currentIndex();
    const bool hovered = underMouse() && r.contains(cursor);
    p.setPen(selected || hovered ? skin.Light.color() : skin.HalfLight.color());
    p.drawText(r, Qt::AlignCenter, tabText(i));

    if(selected)
    {
      p.fillRect(
          QRect{
              r.left() + flatTabPadding / 2, r.bottom() - flatUnderline + 1,
              r.width() - flatTabPadding, flatUnderline},
          skin.Base4.color());
    }
  }
}

ScriptTabWidget::ScriptTabWidget(QWidget* parent)
    : QTabWidget{parent}
    , m_bar{new ScriptTabBar}
{
  setTabBar(m_bar);
}
}
