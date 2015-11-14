#include "EventTransitions.hpp"

namespace Scenario
{
void ClickOnEvent_Transition::onTransition(QEvent *ev)
{
    auto qev = static_cast<ClickOnEvent_Event*>(ev);
    this->state().clear();

    this->state().clickedEvent = qev->id;
    this->state().currentPoint = qev->point;
}

void MoveOnEvent_Transition::onTransition(QEvent *ev)
{
    auto qev = static_cast<MoveOnEvent_Event*>(ev);

    this->state().hoveredEvent = qev->id;
    this->state().currentPoint = qev->point;
}

void ReleaseOnEvent_Transition::onTransition(QEvent *ev)
{
    auto qev = static_cast<ReleaseOnEvent_Event*>(ev);

    this->state().hoveredEvent = qev->id;
    this->state().currentPoint = qev->point;
}
}
