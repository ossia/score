#pragma once
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QStateMachine>
#include <QApplication>
#include <iscore/statemachine/StateMachineUtils.hpp>

template<typename Coordinates>
class ISCORE_LIB_BASE_EXPORT GraphicsSceneTool
{
    public:
        virtual ~GraphicsSceneTool() = default;
        void start()
        {
            if(!localSM().isRunning())
                localSM().start();
        }

        void stop()
        {
            if(localSM().isRunning())
            {
                on_cancel();
                localSM().stop();
            }
        }

        void on_cancel()
        {
            localSM().postEvent(new iscore::Cancel_Event);
            QApplication::processEvents();
        }

    protected:
        GraphicsSceneTool(const QGraphicsScene& scene):
            m_scene{scene}
        {

        }

        QGraphicsItem* itemUnderMouse(const QPointF& point) const
        { return m_scene.itemAt(point, QTransform()); }

        const QGraphicsScene& scene() const { return m_scene; }
        QStateMachine& localSM() { return m_localSM; }

    private:
        const QGraphicsScene& m_scene;
        QStateMachine m_localSM;
};
