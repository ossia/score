#pragma once
#include <Scenario/Document/Minimap/Minimap.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioScene.hpp>
#include <Scenario/Document/ScenarioDocument/TimeBar.hpp>
#include <Scenario/Document/TimeRuler/TimeRuler.hpp>
#include <Scenario/Document/TimeRuler/TimeRulerGraphicsView.hpp>

#include <score/graphics/ArrowDialog.hpp>
#include <score/graphics/GraphicsProxyObject.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateView.hpp>

#include <QGraphicsView>
#include <QMimeData>
#include <QPoint>
#include <QPointer>

#include <score_plugin_scenario_export.h>
#include <verdigris>

class QGraphicsView;
class QObject;
class QWidget;
class ProcessGraphicsView;
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
}

namespace Scenario
{
class Minimap;
class ScenarioScene;
class TimeRuler;
class SCORE_PLUGIN_SCENARIO_EXPORT ProcessGraphicsView final : public QGraphicsView
{
  W_OBJECT(ProcessGraphicsView)
public:
  ProcessGraphicsView(
      const score::GUIApplicationContext& ctx,
      QGraphicsScene* scene,
      QWidget* parent);
  ~ProcessGraphicsView() override;

  void scrollHorizontal(double dx);

  QPointer<score::ArrowDialog> currentPopup{};

public:
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
  //void drawBackground(QPainter* painter, const QRectF& rect) override;

  const score::GUIApplicationContext& m_app;

  std::chrono::steady_clock::time_point m_lastwheel;
  bool m_hZoom{false};
  bool m_vZoom{false};
  bool m_opengl{false};

  // QObject interface
};

class SCORE_PLUGIN_SCENARIO_EXPORT ScenarioDocumentView final
    : public score::DocumentDelegateView
{
  W_OBJECT(ScenarioDocumentView)

public:
  ScenarioDocumentView(const score::DocumentContext& ctx, QObject* parent);
  ~ScenarioDocumentView() override;

  QWidget* getWidget() override;

  BaseGraphicsObject& baseItem() { return m_baseObject; }

  ScenarioScene& scene() { return m_scene; }

  ProcessGraphicsView& view() { return m_view; }

  qreal viewWidth() const;

  QGraphicsView& rulerView() { return m_timeRulerView; }

  TimeRulerBase& timeRuler() { return *m_timeRuler; }

  Minimap& minimap() { return m_minimap; }

  TimeBar& timeBar() { return m_bar; }

  QRectF viewportRect() const;
  QRectF visibleSceneRect() const;

  void showRulers(bool);

public:
  void elementsScaleChanged(double arg_1)
      W_SIGNAL(elementsScaleChanged, arg_1);
  void setLargeView() W_SIGNAL(setLargeView);

private:
  void timerEvent(QTimerEvent* event) override;
  QWidget* m_widget{};
  ScenarioScene m_scene;
  ProcessGraphicsView m_view;
  BaseGraphicsObject m_baseObject;

  QGraphicsScene m_timeRulerScene;
  TimeRulerGraphicsView m_timeRulerView;
  TimeRulerBase* m_timeRuler{};
  QGraphicsScene m_minimapScene;
  MinimapGraphicsView m_minimapView;
  Minimap m_minimap;

  Scenario::TimeBar m_bar;
  int m_timer{};
};
}

Q_DECLARE_METATYPE(const QMimeData*)
W_REGISTER_ARGTYPE(const QMimeData*)
