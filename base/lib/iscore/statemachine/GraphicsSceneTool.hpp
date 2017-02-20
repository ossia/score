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

  const QQuickItem& scene() const
  {
    return m_scene;
  }
  QStateMachine& localSM()
  {
    return m_localSM;
  }

protected:
  GraphicsSceneTool(const QQuickItem& scene) : m_scene{scene}
  {
  }

  QQuickItem* itemUnderMouse(const QPointF& point) const
  {
    return m_scene.childAt(point.x(), point.y());
  }

private:
  const QQuickItem& m_scene;
  QStateMachine m_localSM;
};
