#include "StateTransitions.hpp"

namespace Scenario
{
void ClickOnState_Transition::onTransition(QEvent *ev)
{
    auto qev = static_cast<ClickOnState_Event*>(ev);
    this->state().clear();

    this->state().clickedState= qev->id;
    this->state().currentPoint = qev->point;
}

void MoveOnState_Transition::onTransition(QEvent *ev)
{
    auto qev = static_cast<MoveOnState_Event*>(ev);

    this->state().hoveredState = qev->id;
    this->state().currentPoint = qev->point;
}

void ReleaseOnState_Transition::onTransition(QEvent *ev)
{
    auto qev = static_cast<ReleaseOnState_Event*>(ev);

    this->state().hoveredState = qev->id;
    this->state().currentPoint = qev->point;
}
}
