#include "ConstraintTransitions.hpp"

void ClickOnConstraint_Transition::onTransition(QEvent *ev)
{
    auto qev = static_cast<ClickOnConstraint_Event*>(ev);
    this->state().clear();

    this->state().clickedConstraint = qev->id;
    this->state().currentPoint = qev->point;
}


void MoveOnConstraint_Transition::onTransition(QEvent *ev)
{
    auto qev = static_cast<MoveOnConstraint_Event*>(ev);

    this->state().hoveredConstraint= qev->id;
    this->state().currentPoint = qev->point;
}
