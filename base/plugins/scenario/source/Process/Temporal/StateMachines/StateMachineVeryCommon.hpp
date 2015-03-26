#pragma once
#include <QEvent>
#include <QAbstractTransition>
#include <QState>
#include <ProcessInterface/TimeValue.hpp>

// A coordinate : (t, y)
struct ScenarioPoint
{
        ScenarioPoint() = default;
        ScenarioPoint(const ScenarioPoint&) = default;
        ScenarioPoint(ScenarioPoint&&) = default;

        ScenarioPoint& operator=(const ScenarioPoint&) = default;
        ScenarioPoint& operator=(ScenarioPoint&&) = default;

        TimeValue date;
        double y;
};

template<int N>
struct NumberedEvent : public QEvent
{
        static constexpr const int user_type = N;
        NumberedEvent():
            QEvent{QEvent::Type(QEvent::User + N)} { }
};

struct PositionedEventBase : public QEvent
{
        PositionedEventBase(const ScenarioPoint& pt, QEvent::Type type):
            QEvent{type},
            point(pt)
        { }

        ScenarioPoint point;
};

// We avoid virtual inheritance (with Numbered event);
// this replicates a tiny bit of code.
template<int N>
struct PositionedEvent : public PositionedEventBase
{
        static constexpr const int user_type = N;
        PositionedEvent(const ScenarioPoint& pt):
            PositionedEventBase{pt, QEvent::Type(QEvent::User + N)}
        {
        }
};

template<typename Event>
class MatchedTransition : public QAbstractTransition
{
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

using ScenarioPress_Event = NumberedEvent<1>;
using ScenarioMove_Event = NumberedEvent<2>;
using ScenarioRelease_Event = NumberedEvent<3>;

using ScenarioPress_Transition = MatchedTransition<ScenarioPress_Event>;
using ScenarioMove_Transition = MatchedTransition<ScenarioMove_Event>;
using ScenarioRelease_Transition = MatchedTransition<ScenarioRelease_Event>;
