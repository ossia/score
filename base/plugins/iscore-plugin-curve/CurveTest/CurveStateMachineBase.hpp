#pragma once
#include <QEvent>
#include <QAbstractTransition>

template<int N>
struct NumberedEvent : public QEvent
{
        static constexpr const int user_type = N;
        NumberedEvent():
            QEvent{QEvent::Type(QEvent::User + N)} { }
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

template<typename Transition, typename SourceState, typename TargetState, typename... Args>
Transition* make_transition(SourceState source, TargetState dest, Args&&... args)
{
    auto t = new Transition{std::forward<Args>(args)...};
    t->setTargetState(dest);
    source->addTransition(t);
    return t;
}

using PressPoint_Event = NumberedEvent<1>;
using MovePoint_Event = NumberedEvent<2>;
using ReleasePoint_Event = NumberedEvent<3>;

using PressSegment_Event = NumberedEvent<4>;
using MoveSegment_Event = NumberedEvent<5>;
using ReleaseSegment_Event = NumberedEvent<6>;

using Cancel_Event = NumberedEvent<10>;

using PressPoint_Transition = MatchedTransition<PressPoint_Event>;
using MovePoint_Transition = MatchedTransition<MovePoint_Event>;
using ReleasePoint_Transition = MatchedTransition<ReleasePoint_Event>;

using PressSegment_Transition = MatchedTransition<PressSegment_Event>;
using MoveSegment_Transition = MatchedTransition<MoveSegment_Event>;
using ReleaseSegment_Transition = MatchedTransition<ReleaseSegment_Event>;

using Cancel_Transition = MatchedTransition<Cancel_Event>;
