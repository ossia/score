#pragma once
#include <Process/Tools/ProcessGraphicsView.hpp>
#include <wobjectdefs.h>
#include <Scenario/Document/Minimap/Minimap.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioScene.hpp>
#include <Scenario/Document/ScenarioDocument/TimeBar.hpp>
#include <Scenario/Document/TimeRuler/TimeRuler.hpp>
#include <Scenario/Document/TimeRuler/TimeRulerGraphicsView.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateView.hpp>
#include <score/widgets/GraphicsProxyObject.hpp>

class QGraphicsView;
class QObject;
class QWidget;
class ProcessGraphicsView;

namespace score
{
class DoubleSlider;
struct GUIApplicationContext;
}

namespace Scenario
{
class Minimap;
class ScenarioScene;
class TimeRuler;
class ScenarioDocumentView final : public score::DocumentDelegateView
{
  W_OBJECT(ScenarioDocumentView)

public:
  ScenarioDocumentView(
      const score::GUIApplicationContext& ctx, QObject* parent);
  virtual ~ScenarioDocumentView() = default;

  QWidget* getWidget() override;

  BaseGraphicsObject& baseItem()
  {
    return m_baseObject;
  }

  ScenarioScene& scene()
  {
    return m_scene;
  }

  ProcessGraphicsView& view()
  {
    return m_view;
  }

  qreal viewWidth() const;

  QGraphicsView& rulerView()
  {
    return m_timeRulersView;
  }

  TimeRuler& timeRuler()
  {
    return m_timeRuler;
  }

  Minimap& minimap()
  {
    return m_minimap;
  }

  TimeBar& timeBar()
  {
    return m_bar;
  }

  QRectF viewportRect() const;
  QRectF visibleSceneRect() const;

public:
  void elementsScaleChanged(double arg_1) W_SIGNAL(elementsScaleChanged, arg_1);
  void setLargeView() W_SIGNAL(setLargeView);

private:
  QWidget* m_widget{};
  ScenarioScene m_scene;
  ProcessGraphicsView m_view;
  BaseGraphicsObject m_baseObject;

  QGraphicsScene m_timerulerScene;
  TimeRulerGraphicsView m_timeRulersView;
  TimeRuler m_timeRuler;
  QGraphicsScene m_minimapScene;
  MinimapGraphicsView m_minimapView;
  Minimap m_minimap;

  Scenario::TimeBar m_bar;
};
}
