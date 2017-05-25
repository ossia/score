#pragma once
#include <Scenario/Document/TimeRuler/TimeRulerGraphicsView.hpp>
#include <Scenario/Document/Minimap/Minimap.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioScene.hpp>
#include <iscore/widgets/GraphicsProxyObject.hpp>
#include <Scenario/Document/TimeRuler/MainTimeRuler/TimeRulerView.hpp>

#include <Process/Tools/ProcessGraphicsView.hpp>

#include <iscore/plugins/documentdelegate/DocumentDelegateView.hpp>

class QGraphicsView;
class QObject;
class QWidget;
class ProcessGraphicsView;

namespace iscore
{
class DoubleSlider;
struct GUIApplicationContext;
}

namespace Scenario
{
class Minimap;
class ScenarioScene;
class TimeRulerView;
class ScenarioDocumentView final : public iscore::DocumentDelegateView
{
  Q_OBJECT

public:
  ScenarioDocumentView(const iscore::GUIApplicationContext& ctx, QObject* parent);
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

  TimeRulerView& timeRuler()
  {
    return m_timeRuler;
  }

  Minimap& minimap()  { return m_minimap; }


  QRectF viewportRect() const;
  QRectF visibleSceneRect() const;

signals:
  void elementsScaleChanged(double);

private:
  QWidget* m_widget{};
  ScenarioScene m_scene;
  ProcessGraphicsView m_view;
  BaseGraphicsObject m_baseObject;

  TimeRulerGraphicsView m_timeRulersView;
  TimeRulerView m_timeRuler;
  QGraphicsScene m_minimapScene;
  TimeRulerGraphicsView m_minimapView;
  Minimap m_minimap;
};
}
