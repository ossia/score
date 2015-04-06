#pragma once
#include <iscore/command/OngoingCommandManager.hpp>
#include "Process/Temporal/StateMachines/StateMachineCommon.hpp"

#include <iscore/selection/SelectionDispatcher.hpp>

class TemporalScenarioPresenter;
class SelectAreaState : public QState
{
    public:
        SelectAreaState(const TemporalScenarioPresenter& presenter,
                        iscore::SelectionDispatcher& dispatcher);

    private:
        void setSelectionArea(const QRectF& area);

        const TemporalScenarioPresenter& m_presenter;
        iscore::SelectionDispatcher& m_dispatcher;
};
