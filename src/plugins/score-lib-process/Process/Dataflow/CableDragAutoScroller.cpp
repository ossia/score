#include "CableDragAutoScroller.hpp"

#include <QGraphicsView>
#include <QScrollBar>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace Dataflow
{
CableDragAutoScroller::CableDragAutoScroller(
    std::function<void(QPointF scenePos)> onScrolled)
    : m_onScrolled{std::move(onScrolled)}
{
  m_timer.setInterval(intervalMs);
  m_timer.setTimerType(Qt::PreciseTimer);
  connect(&m_timer, &QTimer::timeout, this, &CableDragAutoScroller::tick);
}

CableDragAutoScroller::~CableDragAutoScroller() = default;

QPoint CableDragAutoScroller::scrollStep(QPoint viewportPos, QSize sz) noexcept
{
  auto axis = [](int pos, int extent) noexcept -> int {
    if(extent <= 2 * margin)
      return 0;
    if(pos < margin)
    {
      const double depth = double(margin - std::max(pos, 0)) / margin;
      return -std::clamp(int(std::lround(depth * maxStep)), 1, maxStep);
    }
    if(pos >= extent - margin)
    {
      const double depth = double(std::min(pos, extent - 1) - (extent - margin) + 1) / margin;
      return std::clamp(int(std::lround(depth * maxStep)), 1, maxStep);
    }
    return 0;
  };
  return QPoint{axis(viewportPos.x(), sz.width()), axis(viewportPos.y(), sz.height())};
}

QGraphicsView* CableDragAutoScroller::view() const noexcept
{
  return m_view;
}

void CableDragAutoScroller::track(QWidget* viewport, QPointF scenePos)
{
  m_view = viewport ? qobject_cast<QGraphicsView*>(viewport->parentWidget()) : nullptr;
  if(!m_view)
  {
    stop();
    return;
  }

  m_viewportPos = m_view->mapFromScene(scenePos);
  if(scrollStep(m_viewportPos, m_view->viewport()->size()).isNull())
    m_timer.stop();
  else if(!m_timer.isActive())
    m_timer.start();
}

void CableDragAutoScroller::stop()
{
  m_timer.stop();
  m_view = nullptr;
}

void CableDragAutoScroller::tick()
{
  if(!m_view)
  {
    stop();
    return;
  }

  const QPoint step = scrollStep(m_viewportPos, m_view->viewport()->size());
  if(step.isNull())
  {
    m_timer.stop();
    return;
  }

  bool moved = false;
  auto scroll = [&moved](QScrollBar* bar, int delta) {
    if(!bar || delta == 0)
      return;
    const int old = bar->value();
    bar->setValue(old + delta);
    moved |= bar->value() != old;
  };
  scroll(m_view->horizontalScrollBar(), step.x());
  scroll(m_view->verticalScrollBar(), step.y());

  if(!moved)
  {
    // Nothing left to scroll that way: idle until the next drag-move event.
    m_timer.stop();
    return;
  }

  if(m_onScrolled)
    m_onScrolled(m_view->mapToScene(m_viewportPos));
}
}
