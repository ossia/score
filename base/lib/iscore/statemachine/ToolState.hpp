#pragma once
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QStateMachine>
#include <iscore/statemachine/StateMachineUtils.hpp>

/**
 * @brief The ToolState class
 *
 * A generic state to implement an edition tool.
 */
class ToolState : public QState
{
    public:
        virtual ~ToolState();
        void start();
        void stop();

    protected:
        ToolState(const QGraphicsScene& scene, QState*);

        virtual void on_pressed() = 0;
        virtual void on_moved() = 0;
        virtual void on_released() = 0;

        QGraphicsItem* itemUnderMouse(const QPointF& point) const;

        const QGraphicsScene& scene() const { return m_scene; }
        QStateMachine& localSM() { return m_localSM; }

    private:
        const QGraphicsScene& m_scene;
        QStateMachine m_localSM;
};

template<typename Coordinates>
class GraphicsSceneToolBase
{
    public:
        virtual ~GraphicsSceneToolBase() = default;
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
        }

    protected:
        GraphicsSceneToolBase(const QGraphicsScene& scene):
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
