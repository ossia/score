#pragma once
#include "CurveStateMachineData.hpp"
#include "CurvePoint.hpp"
#include <iscore/statemachine/BaseStateMachine.hpp>
class CurvePresenter;
class QGraphicsScene;
class CurveModel;
class CurveStateMachine : public BaseStateMachine
{
    public:
        CurveStateMachine(CurvePresenter& pres, QObject* parent);
        const CurvePresenter& presenter() const;
        const CurveModel& model() const;

    private:
        void setupPostEvents();
        void setupStates();
        CurvePresenter& m_presenter;
        CurvePoint curvePoint;

        // Tools
        QState* m_selectTool{};

        QState* m_createTool{};
        QState* m_moveTool{};

        QState* m_createPenTool{};
        QState* m_removePenTool{};
};
