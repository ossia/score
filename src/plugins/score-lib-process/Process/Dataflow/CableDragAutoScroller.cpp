#include "CableDragAutoScroller.hpp"

#include "AutoScrollableView.hpp"

#include <QCursor>
#include <QGraphicsView>
#include <QScrollBar>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace Dataflow
{
CableDragAutoScroller::CableDragAutoScroller(
    std::function<void(QPointF scenePos)> onScrolled, CursorProvider cursor)
    : m_onScrolled{std::move(onScrolled)}
    , m_cursor{std::move(cursor)}
{
  if(!m_cursor)
    m_cursor = [](QWidget& viewport) { return viewport.mapFromGlobal(QCursor::pos()); };

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
    if(pos < -outsideLimit || pos >= extent + outsideLimit)
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

QPoint CableDragAutoScroller::cursorInViewport() const
{
  return m_cursor(*m_view->viewport());
}

void CableDragAutoScroller::track(QWidget* viewport, QPointF scenePos)
{
  m_view = viewport ? qobject_cast<QGraphicsView*>(viewport->parentWidget()) : nullptr;
  if(!m_view)
  {
    stop();
    return;
  }

  // The event's position is authoritative for this move; the timer then
  // follows the cursor itself, which is all there is once it leaves the view.
  if(scrollStep(m_view->mapFromScene(scenePos), m_view->viewport()->size()).isNull())
    m_timer.stop();
  else if(!m_timer.isActive())
    m_timer.start();
}

void CableDragAutoScroller::continueFromCursor()
{
  if(m_view && !m_timer.isActive())
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

  const QPoint pos = cursorInViewport();
  const QPoint step = scrollStep(pos, m_view->viewport()->size());
  if(step.isNull())
  {
    // Back inside, or too far away: idle until the next drag-move / leave.
    m_timer.stop();
    return;
  }

  if(!scrollViewBy(*m_view, step))
  {
    // Nothing left to scroll that way: idle until the next drag-move event.
    m_timer.stop();
    return;
  }

  if(m_onScrolled)
    m_onScrolled(m_view->mapToScene(pos));
}

bool CableDragAutoScroller::scrollBarsBy(QGraphicsView& view, QPoint delta)
{
  bool moved = false;
  auto scroll = [&moved](QScrollBar* bar, int d) {
    if(!bar || d == 0)
      return;
    const int old = bar->value();
    bar->setValue(old + d);
    moved |= bar->value() != old;
  };
  scroll(view.horizontalScrollBar(), delta.x());
  scroll(view.verticalScrollBar(), delta.y());
  return moved;
}

bool CableDragAutoScroller::scrollViewBy(QGraphicsView& view, QPoint delta)
{
  if(auto scrollable = dynamic_cast<AutoScrollableView*>(&view))
    return scrollable->autoScrollBy(delta);
  return scrollBarsBy(view, delta);
}

AutoScrollableView::~AutoScrollableView() = default;
}
