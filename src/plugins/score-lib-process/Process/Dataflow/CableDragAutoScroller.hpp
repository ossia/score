#pragma once
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QSize>
#include <QTimer>

#include <score_lib_process_export.h>

#include <functional>

class QGraphicsView;
class QWidget;

namespace Dataflow
{
/**
 * @brief Scrolls a QGraphicsView while a cable is being dragged towards
 * one of its edges, so that a port currently off-screen can be reached.
 *
 * A QDrag runs its own event loop and only reports drag-move events when the
 * cursor actually moves: a timer keeps the view scrolling while the cursor is
 * parked against an edge. After each scroll step the scene point under the
 * (unmoved) cursor has changed, so `onScrolled` is invoked with the new scene
 * position to let the caller update whatever follows the cursor (the drag
 * line, the magnetic target port...).
 */
class SCORE_LIB_PROCESS_EXPORT CableDragAutoScroller final : public QObject
{
public:
  //! Distance from the viewport edge, in pixels, from which scrolling starts.
  static constexpr int margin = 32;
  //! Scroll step per tick, in pixels, reached at the very edge.
  static constexpr int maxStep = 24;
  static constexpr int intervalMs = 16;

  explicit CableDragAutoScroller(std::function<void(QPointF scenePos)> onScrolled);
  ~CableDragAutoScroller();

  /**
   * @brief Scroll step for a cursor at viewportPos inside a viewport of size sz.
   *
   * Zero outside the margins; otherwise grows linearly from 1 px at the inner
   * border of the margin to maxStep at the edge. Viewports too small to have
   * a useful middle never scroll.
   */
  static QPoint scrollStep(QPoint viewportPos, QSize sz) noexcept;

  /**
   * @brief To be called on each drag-move event.
   * @param viewport The widget the scene event was sent to (the view's viewport).
   * @param scenePos The cursor position in scene coordinates.
   */
  void track(QWidget* viewport, QPointF scenePos);

  //! Stops scrolling, e.g. on drop or when the drag leaves the view.
  void stop();

  bool active() const noexcept { return m_timer.isActive(); }
  QGraphicsView* view() const noexcept;

private:
  void tick();

  std::function<void(QPointF)> m_onScrolled;
  QPointer<QGraphicsView> m_view{};
  QPoint m_viewportPos{};
  QTimer m_timer;
};
}
