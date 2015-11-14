#include "NothingTransitions.hpp"
namespace Scenario
{
void ClickOnNothing_Transition::onTransition(QEvent *ev)
{
    auto qev = static_cast<ClickOnNothing_Event*>(ev);
    this->state().clear();

    this->state().currentPoint = qev->point;
}

void MoveOnNothing_Transition::onTransition(QEvent *ev)
{
    auto qev = static_cast<MoveOnNothing_Event*>(ev);

    this->state().currentPoint = qev->point;
}

void ReleaseOnNothing_Transition::onTransition(QEvent *ev)
{
    auto qev = static_cast<ReleaseOnNothing_Event*>(ev);

    this->state().currentPoint = qev->point;
}
}
