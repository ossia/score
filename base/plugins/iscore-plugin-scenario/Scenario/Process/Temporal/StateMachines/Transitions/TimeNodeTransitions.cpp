#include "TimeNodeTransitions.hpp"

namespace Scenario
{
void ClickOnTimeNode_Transition::onTransition(QEvent *ev)
{
    auto qev = static_cast<ClickOnTimeNode_Event*>(ev);
    this->state().clear();

    this->state().clickedTimeNode = qev->id;
    this->state().currentPoint = qev->point;
}


void MoveOnTimeNode_Transition::onTransition(QEvent *ev)
{
    auto qev = static_cast<MoveOnTimeNode_Event*>(ev);

    this->state().hoveredTimeNode = qev->id;
    this->state().currentPoint = qev->point;
}


void ReleaseOnTimeNode_Transition::onTransition(QEvent *ev)
{
    auto qev = static_cast<ReleaseOnTimeNode_Event*>(ev);

    this->state().hoveredTimeNode = qev->id;
    this->state().currentPoint = qev->point;
}
}
