#pragma once
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp"

class ClickOnSlotOverlay_Transition : public MatchedTransition<ClickOnSlotOverlay_Event>
{
    public:
        ClickOnSlotOverlay_Transition(SlotState& state);

        SlotState& state() const;

    protected:
        virtual void onTransition(QEvent * ev) override;

    private:
        SlotState& m_state;
};

class ClickOnSlotHandle_Transition : public MatchedTransition<ClickOnSlotHandle_Event>
{
    public:
        ClickOnSlotHandle_Transition(SlotState& state);

        SlotState& state() const;

    protected:
        virtual void onTransition(QEvent * ev) override;

    private:
        SlotState& m_state;
};
