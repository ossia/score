#pragma once
#include "CurveStateMachineData.hpp"
#include "CurvePoint.hpp"
#include <iscore/statemachine/BaseStateMachine.hpp>
#include <core/command/CommandStack.hpp>
#include <iscore/locking/ObjectLocker.hpp>
class CurvePresenter;
class QGraphicsScene;
class CurveModel;

namespace Curve {
enum class Tool {
    Create, Move, Select
};
}

class CurveStateMachine : public BaseStateMachine
{
        Q_OBJECT
    public:
        CurveStateMachine(CurvePresenter& pres, QObject* parent);
        CurvePresenter& presenter() const;
        const CurveModel& model() const;

        iscore::CommandStack& commandStack() const;
        iscore::ObjectLocker& locker() const;

        CurvePoint curvePoint;

        void changeTool(int state);

    signals:
        void setCreateState();
        void setSelectState();
        void setMoveState();

        void exitState();

    private:
        void setupPostEvents();
        void setupStates();
        CurvePresenter& m_presenter;

        // Tools
        QState* m_transitionState{};

        QState* m_selectTool{};
        QState* m_createTool{};
        QState* m_moveTool{};

        QState* m_createPenTool{};
        QState* m_removePenTool{};

        iscore::CommandStack& m_stack;
        iscore::ObjectLocker& m_locker;
};
