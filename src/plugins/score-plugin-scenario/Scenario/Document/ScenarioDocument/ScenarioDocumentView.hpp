#pragma once
#include <Process/Dataflow/AutoScrollableView.hpp>

#include <Scenario/Document/Minimap/Minimap.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioScene.hpp>
#include <Scenario/Document/TimeRuler/TimeRuler.hpp>
#include <Scenario/Document/TimeRuler/TimeRulerGraphicsView.hpp>

#include <score/graphics/ArrowDialog.hpp>
#include <score/graphics/GraphicsProxyObject.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateView.hpp>
#include <score/tools/Events.hpp>
#include <score/widgets/MimeData.hpp>

#include <ossia/detail/flat_set.hpp>

#include <QGraphicsView>
#include <QMimeData>
#include <QPoint>
#include <QPointer>

#include <score_plugin_scenario_export.h>

#include <verdigris>

#include <functional>

class QGraphicsView;
class QObject;
class QWidget;
class QFocusEvent;
class QGraphicsScene;
class QKeyEvent;
class QPainterPath;
class QResizeEvent;
class QSize;
class QWheelEvent;
class SceneGraduations;

namespace score
{
struct DocumentContext;
struct GUIApplicationContext;

class BackgroundRenderer;
}

namespace Scenario
{
class Minimap;
class ScenarioScene;
class IntervalDurations;
class IntervalView;
class TimeRuler;
class ScenarioDocumentView;
class SCORE_PLUGIN_SCENARIO_EXPORT ProcessGraphicsView final
    : public QGraphicsView
    , public Dataflow::AutoScrollableView
{
  W_OBJECT(ProcessGraphicsView)
public:
  ProcessGraphicsView(
      const score::GUIApplicationContext& ctx, QGraphicsScene* scene, QWidget* parent);
  ~ProcessGraphicsView() override;

  void scrollHorizontal(double dx);
  QRectF visibleRect() const noexcept;

  /**
   * @brief How the content is moved when something (a cable being dragged
   * against the edge) asks to reveal more of it.
   *
   * Set by the central display in charge: the nodal canvas pans its node
   * container, the timeline grows past its end. Without one, the scroll bars
   * move, which only goes as far as the current scene rect.
   */
  std::function<bool(QPoint)> autoScrollHandler;
  bool autoScrollBy(QPoint delta) override;

  QPointer<score::ArrowDialog> currentPopup{};

  IntervalDurations* currentTimebar{};
  IntervalView* currentView{};

  score::BackgroundRenderer* currentBackground{};
  bool timebarPlaying{};
  bool timebarVisible{};

public:
  void drawForeground(QPainter* painter, const QRectF& rect) override;
  void sizeChanged(const QSize& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, sizeChanged, arg_1)
  void scrolled(int arg_1) E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, scrolled, arg_1)
  void focusedOut() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, focusedOut)
  void horizontalZoom(QPointF pixDelta, QPointF pos)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, horizontalZoom, pixDelta, pos)
  void verticalZoom(QPointF pixDelta, QPointF pos)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, verticalZoom, pixDelta, pos)

  void visibleRectChanged(QRectF r)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, visibleRectChanged, r)
  void dropRequested(QPoint pos, const QMimeData* mime)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, dropRequested, pos, mime)
  void emptyContextMenuRequested(QPoint pos)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, emptyContextMenuRequested, pos)

  void dropFinished() W_SIGNAL(dropFinished);

  void mousePress(QMouseEvent* p) E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, mousePress, p);
  void mouseMove(QMouseEvent* p) E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, mouseMove, p);
  void mouseRelease(QMouseEvent* p)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, mouseRelease, p);
  void hoverEnter(QHoverEvent* p) E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, hoverEnter, p);
  void hoverMove(QHoverEvent* p) E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, hoverMove, p);
  void hoverLeave(QHoverEvent* p) E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, hoverLeave, p);
  void keyPress(QKeyEvent* e) E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, keyPress, e);
  void keyRelease(QKeyEvent* e) E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, keyRelease, e);
  void tabletMove(QTabletEvent* e) E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, tabletMove, e);

private:
  void resizeEvent(QResizeEvent* ev) override;
  void scrollContentsBy(int dx, int dy) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;
  void focusOutEvent(QFocusEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dragLeaveEvent(QDragLeaveEvent* event) override;
  void dropEvent(QDropEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  bool event(QEvent*) override;

  void hoverEnterEvent(QHoverEvent* event);
  void hoverMoveEvent(QHoverEvent* event);
  void hoverLeaveEvent(QHoverEvent* event);

  void checkAndRemoveCurrentDialog(QPoint pos);
  void drawBackground(QPainter* painter, const QRectF& rect) override;
  const score::GUIApplicationContext& m_app;

  std::chrono::steady_clock::time_point m_lastwheel;
  ossia::flat_set<Qt::MouseButton> m_press_release_chain{};
  std::vector<score::BackgroundRenderer*> m_globalRenderers;
  bool m_opengl{false};

  friend class ScenarioDocumentView;
};

class SCORE_PLUGIN_SCENARIO_EXPORT ScenarioDocumentView final
    : public score::DocumentDelegateView
{
  W_OBJECT(ScenarioDocumentView)

public:
  ScenarioDocumentView(const score::DocumentContext& ctx, QObject* parent);
  ~ScenarioDocumentView() override;

  QWidget* getWidget() override;

  BaseGraphicsObject& baseItem() { return *m_baseObject; }

  ScenarioScene& scene() { return m_scene; }

  ProcessGraphicsView& view() { return *m_view; }

  qreal viewWidth() const;

  QGraphicsView& rulerView() { return *m_timeRulerView; }

  TimeRulerBase& timeRuler() { return *m_timeRuler; }

  Minimap& minimap() { return *m_minimap; }

  QRectF viewportRect() const;
  QRectF visibleSceneRect() const;

  void showRulers(bool);
  void ready() override;

  void zoom(double zx, double zy);
  void scroll(double dx, double dy);

  void addBackgroundRenderer(score::BackgroundRenderer*);
  void removeBackgroundRenderer(score::BackgroundRenderer*);
  void updateBackgroundMode();

  void elementsScaleChanged(double arg_1) W_SIGNAL(elementsScaleChanged, arg_1);
  void setLargeView() W_SIGNAL(setLargeView);
  void timeRulerChanged() W_SIGNAL(timeRulerChanged);
  void requestTransport(QPointF pt) W_SIGNAL(requestTransport, pt);

private:
  void timerEvent(QTimerEvent* event) override;
  QWidget* m_widget{};
  const score::DocumentContext& m_context;
  ScenarioScene m_scene;

  // The three views below are put into m_widget's layout, which REPARENTS
  // them: Qt then deletes them through ~QWidget's deleteChildren(). As
  // by-value members that was `delete` on an address inside this object,
  // which is not a heap allocation -- the same defect 6227fbd07f fixed for
  // the scenes and the base item, and the reason live-edit-sweep.sh had to
  // carve their four destructors out of its ASan reports by name.
  //
  // Heap-allocated, they are Qt's to delete, and QPointer means we can also
  // delete them ourselves in ~ScenarioDocumentView for the OTHER teardown
  // order (a document closed with removeTab orphans the widget, so nothing
  // Qt-side ever deletes it) without risking a double free. Deleting them in
  // the destructor BODY also keeps the original ordering guarantee: a view
  // dies before the by-value scene it renders.
  QPointer<ProcessGraphicsView> m_view;
  // Owned by the scene, not by value. m_scene is a QObject child of m_widget,
  // so ~QWidget destroys it -- and QGraphicsScene::clear() deletes its
  // top-level items -- before ScenarioDocumentView's own members are destroyed.
  // As a by-value member that was `delete` on a non-heap address.
  BaseGraphicsObject* m_baseObject{};

  QGraphicsScene m_timeRulerScene;
  QPointer<TimeRulerGraphicsView> m_timeRulerView;
  TimeRulerBase* m_timeRuler{};
  QGraphicsScene m_minimapScene;
  QPointer<MinimapGraphicsView> m_minimapView;
  // Same treatment as m_baseObject: a QGraphicsItem added to a scene that
  // deletes its top-level items, so it must be heap-allocated and left to
  // that scene. Never deleted here.
  Minimap* m_minimap{};

  int m_timer{-1};
  bool m_transport{};
};
}
