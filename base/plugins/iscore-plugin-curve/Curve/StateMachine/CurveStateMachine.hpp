#pragma once
#include <Curve/StateMachine/States/Tools/MoveTool.hpp>
#include <Curve/StateMachine/States/Tools/SelectionTool.hpp>
#include <Curve/StateMachine/CurveStateMachineData.hpp>
#include <Curve/StateMachine/CurvePoint.hpp>
#include <Curve/StateMachine/CurveEditionSettings.hpp>

#include <iscore/statemachine/BaseStateMachine.hpp>
#include <iscore/locking/ObjectLocker.hpp>

#include <core/command/CommandStack.hpp>

class CurvePresenter;
class QGraphicsScene;
class CurveModel;

namespace Curve
{
class EditionToolForSelection;
class ToolPalette final : public GraphicsSceneToolPalette
{
        Q_OBJECT
    public:
        ToolPalette(CurvePresenter& pres, QObject* parent);
        CurvePresenter& presenter() const;
        const CurveModel& model() const;

        iscore::CommandStack& commandStack() const;
        iscore::ObjectLocker& locker() const;

        Curve::Point curvePoint;

        void on_pressed(QPointF);
        void on_moved(QPointF);
        void on_released(QPointF);
        void on_cancel();

    private:
        void changeTool(Curve::Tool);
        Curve::Point ScenePointToCurvePoint(const QPointF& point);
        void setupPostEvents();
        CurvePresenter& m_presenter;

        // Tools
        QState* m_transitionState{};

//        QState* m_createPenTool{};
//        QState* m_removePenTool{};

        iscore::CommandStack& m_stack;
        iscore::ObjectLocker& m_locker;

        SelectionAndMoveTool m_selectTool;
        CreateTool m_createTool;
        SetSegmentTool m_setSegmentTool;
};
}
