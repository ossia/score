#pragma once
#include <QApplication>
#include <QQuickPaintedItem>
#include <QGraphicsScene>
#include <QStateMachine>
#include <iscore/statemachine/StateMachineUtils.hpp>

template <typename Coordinates>
class GraphicsSceneTool
{
public:
  virtual ~GraphicsSceneTool() = default;
  void start()
  {
    if (!localSM().isRunning())
      localSM().start();
  }

  void stop()
  {
    if (localSM().isRunning())
    {
      on_cancel();
      localSM().stop();
    }
  }

  virtual void on_cancel()
  {
    localSM().postEvent(new iscore::Cancel_Event);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  }

  const QGraphicsScene& scene() const
  {
    return m_scene;
  }
  QStateMachine& localSM()
  {
    return m_localSM;
  }

protected:
  GraphicsSceneTool(const QGraphicsScene& scene) : m_scene{scene}
  {
  }

  QQuickPaintedItem* itemUnderMouse(const QPointF& point) const
  {
    return m_scene.itemAt(point, QTransform());
  }

private:
  const QGraphicsScene& m_scene;
  QStateMachine m_localSM;
};
