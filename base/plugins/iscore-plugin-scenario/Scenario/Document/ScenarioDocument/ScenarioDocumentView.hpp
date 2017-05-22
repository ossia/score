#pragma once
#include <iscore/plugins/documentdelegate/DocumentDelegateView.hpp>

class BaseGraphicsObject;
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
class ScenarioScene;
class TimeRulerView;
class ScenarioDocumentView final : public iscore::DocumentDelegateView
{
  Q_OBJECT

public:
  ScenarioDocumentView(const iscore::GUIApplicationContext& ctx, QObject* parent);
  virtual ~ScenarioDocumentView() = default;

  QWidget* getWidget() override;

  BaseGraphicsObject* baseItem() const
  {
    return m_baseObject;
  }

  void update();

  ScenarioScene& scene() const
  {
    return *m_scene;
  }

  ProcessGraphicsView& view() const
  {
    return *m_view;
  }

  qreal viewWidth() const;

  QGraphicsView* rulerView() const
  {
    return m_timeRulersView;
  }

  TimeRulerView* timeRuler()
  {
    return m_timeRuler;
  }

  iscore::DoubleSlider* zoomSlider() const
  {
    return m_zoomSlider;
  }

  void setLargeView();
signals:
  void horizontalZoomChanged(double newZoom);
  void elementsScaleChanged(double);

private:
  QWidget* m_widget{};
  ScenarioScene* m_scene{};
  ProcessGraphicsView* m_view{};
  BaseGraphicsObject* m_baseObject{};

  QGraphicsView* m_timeRulersView{};
  TimeRulerView* m_timeRuler{};

  iscore::DoubleSlider* m_zoomSlider{};
};
}
