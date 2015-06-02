#pragma once
#include <iscore/tools/ObjectPath.hpp>

#include <QState>
#include <QEvent>
#include <QAbstractTransition>

template<int N>
struct NumberedEvent : public QEvent
{
        static constexpr const int user_type = N;
        NumberedEvent():
            QEvent{QEvent::Type(QEvent::User + N)} { }
};

template<typename Element, int N>
struct NumberedWithPath_Event : public NumberedEvent<N>
{
        NumberedWithPath_Event(const ObjectPath& p):
            NumberedEvent<N>{},
            path{p}
        {
        }

        ObjectPath path;
};

template<typename Event>
class MatchedTransition : public QAbstractTransition
{
    public:
        using event_type = Event;

    protected:
        virtual bool eventTest(QEvent *e) override
        { return e->type() == QEvent::Type(QEvent::User + Event::user_type); }

        virtual void onTransition(QEvent *event) override { }
};

template<typename State, typename T>
class StateAwareTransition : public T
{
    public:
        StateAwareTransition(State& state):
                    m_state{state} { }

        State& state() const { return m_state; }

    private:
        State& m_state;
};

template<typename Transition, typename SourceState, typename TargetState, typename... Args>
Transition* make_transition(SourceState source, TargetState dest, Args&&... args)
{
    auto t = new Transition{std::forward<Args>(args)...};
    t->setTargetState(dest);
    source->addTransition(t);
    return t;
}

enum Modifier
{ Click = 100, Move = 200, Release = 300 };

using Press_Event = NumberedEvent<1>;
using Move_Event = NumberedEvent<2>;
using Release_Event = NumberedEvent<3>;
using Cancel_Event = NumberedEvent<4>;
using Shift_Event = NumberedEvent<5>;

using Press_Transition = MatchedTransition<Press_Event>;
using Move_Transition = MatchedTransition<Move_Event>;
using Release_Transition = MatchedTransition<Release_Event>;
using Cancel_Transition = MatchedTransition<Cancel_Event>;
using ShiftTransition = MatchedTransition<Shift_Event>;
