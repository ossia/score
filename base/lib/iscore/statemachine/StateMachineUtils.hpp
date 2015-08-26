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
        explicit NumberedWithPath_Event(const ObjectPath& p):
            NumberedEvent<N>{},
            path{p}
        {
        }

        explicit NumberedWithPath_Event(ObjectPath&& p):
            NumberedEvent<N>{},
            path{std::move(p)}
        {
        }

        ObjectPath path;
};

template<typename PointType>
struct PositionedEvent : public QEvent
{
        PositionedEvent(
                const PointType& pt,
                QEvent::Type type):
            QEvent{type},
            point{pt}
        {

        }

        PointType point;
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
        explicit StateAwareTransition(State& state):
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

namespace Modifier {
    struct Click_tag{ static constexpr const int value = 100; };
    struct Move_tag  { static constexpr const int value = 200; };
    struct Release_tag{ static constexpr const int value = 300; };
}
enum Modifier_tagme
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
