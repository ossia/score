#pragma once
#include <Scenario/Palette/ScenarioPaletteBaseTransitions.hpp>
namespace Scenario
{
template<typename Scenario_T>
class MoveOnAnything_Transition final : public GenericTransition<Scenario_T, QAbstractTransition>
{
    public:
        using GenericTransition<Scenario_T, QAbstractTransition>::GenericTransition;

    protected:
        bool eventTest(QEvent *e) override
        {
            using namespace std;
            static const constexpr QEvent::Type types[] = {
                QEvent::Type(QEvent::User + MoveOnNothing_Event::user_type),
                QEvent::Type(QEvent::User + MoveOnState_Event::user_type),
                QEvent::Type(QEvent::User + MoveOnEvent_Event::user_type),
                QEvent::Type(QEvent::User + MoveOnTimeNode_Event::user_type),
                QEvent::Type(QEvent::User + MoveOnConstraint_Event::user_type),
                QEvent::Type(QEvent::User + MoveOnLeftBrace_Event::user_type),
                QEvent::Type(QEvent::User + MoveOnRightBrace_Event::user_type)};

            return find(begin(types), end(types), e->type()) != end(types);
        }

        void onTransition(QEvent *event) override
        {
            auto qev = static_cast<iscore::PositionedEvent<Scenario::Point>*>(event);

            this->state().currentPoint = qev->point;
        }
};

class ReleaseOnAnything_Transition final : public QAbstractTransition
{
    protected:
        bool eventTest(QEvent *e) override
        {
            using namespace std;
            static const constexpr QEvent::Type types[] = {
                QEvent::Type(QEvent::User + ReleaseOnNothing_Event::user_type),
                QEvent::Type(QEvent::User + ReleaseOnState_Event::user_type),
                QEvent::Type(QEvent::User + ReleaseOnEvent_Event::user_type),
                QEvent::Type(QEvent::User + ReleaseOnTimeNode_Event::user_type),
                QEvent::Type(QEvent::User + ReleaseOnConstraint_Event::user_type),
                QEvent::Type(QEvent::User + ReleaseOnLeftBrace_Event::user_type),
                QEvent::Type(QEvent::User + ReleaseOnRightBrace_Event::user_type)};

            return find(begin(types), end(types), e->type()) != end(types);
        }
        void onTransition(QEvent *event) override
        {

        }
};
}
