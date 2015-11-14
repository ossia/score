#include "SlotTransitions.hpp"
namespace Scenario
{
ClickOnSlotOverlay_Transition::ClickOnSlotOverlay_Transition(
        SlotState &state):
    m_state{state}
{

}

SlotState &ClickOnSlotOverlay_Transition::state() const
{
    return m_state;
}

void ClickOnSlotOverlay_Transition::onTransition(QEvent *ev)
{
    auto qev = static_cast<event_type*>(ev);

    this->state().currentSlot = std::move(qev->path);
}


ClickOnSlotHandle_Transition::ClickOnSlotHandle_Transition(
        SlotState &state):
    m_state{state}
{

}


SlotState &ClickOnSlotHandle_Transition::state() const
{
    return m_state;
}


void ClickOnSlotHandle_Transition::onTransition(QEvent *ev)
{
    auto qev = static_cast<event_type*>(ev);

    this->state().currentSlot = std::move(qev->path);
}
}
