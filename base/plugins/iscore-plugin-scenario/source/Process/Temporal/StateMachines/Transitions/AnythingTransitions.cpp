#include "AnythingTransitions.hpp"
bool MoveOnAnything_Transition::eventTest(QEvent *e)
{
    using namespace std;
    static const constexpr QEvent::Type types[] = {
        QEvent::Type(QEvent::User + MoveOnNothing_Event::user_type),
        QEvent::Type(QEvent::User + MoveOnEvent_Event::user_type),
        QEvent::Type(QEvent::User + MoveOnTimeNode_Event::user_type),
        QEvent::Type(QEvent::User + MoveOnConstraint_Event::user_type)};

    return find(begin(types), end(types), e->type()) != end(types);
}

void MoveOnAnything_Transition::onTransition(QEvent *event)
{
    auto qev = static_cast<PositionedEvent<ScenarioPoint>*>(event);

    this->state().currentPoint = qev->point;
}


bool ReleaseOnAnything_Transition::eventTest(QEvent *e)
{
    using namespace std;
    static const constexpr QEvent::Type types[] = {
        QEvent::Type(QEvent::User + ReleaseOnNothing_Event::user_type),
        QEvent::Type(QEvent::User + ReleaseOnEvent_Event::user_type),
        QEvent::Type(QEvent::User + ReleaseOnTimeNode_Event::user_type),
        QEvent::Type(QEvent::User + ReleaseOnConstraint_Event::user_type)};

    return find(begin(types), end(types), e->type()) != end(types);
}


void ReleaseOnAnything_Transition::onTransition(QEvent *event) { }
