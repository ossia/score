#pragma once
#include "CurveStateMachineData.hpp"
#include <QStateMachine>
class CurvePresenter;
class CurveStateMachine : public QStateMachine
{
    public:
        CurveStateMachine(CurvePresenter& pres, QObject* parent);

    private:
        CurvePresenter& m_presenter;
        CurveStateMachineData m_data;

        // Tools
        QState* m_selectTool{};
        QState* m_moveTool{};
        QState* m_createTool{};

        QState* m_createPenTool{};
        QState* m_removePenTool{};
};
