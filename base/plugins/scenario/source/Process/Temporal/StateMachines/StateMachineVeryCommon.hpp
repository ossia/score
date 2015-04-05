#pragma once
#include <QEvent>
#include <QAbstractTransition>
#include <ProcessInterface/TimeValue.hpp>

// A coordinate : (t, y)
struct ScenarioPoint
{
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

template<int N>
struct PositionedEvent : public NumberedEvent<N>
{
        PositionedEvent(const ScenarioPoint& pt):
            point(pt)
        {
        }

        ScenarioPoint point;
};

template<typename Event>
class MatchedTransition : public QAbstractTransition
{
    protected:
        virtual bool eventTest(QEvent *e) override
        { return e->type() == QEvent::Type(QEvent::User + Event::user_type); }

        virtual void onTransition(QEvent *event) override { }
};


// Not specialized
using ScenarioPress_Event = NumberedEvent<1>;
using ScenarioMove_Event = NumberedEvent<2>;
using ScenarioRelease_Event = NumberedEvent<3>;

using ScenarioPress_Transition = MatchedTransition<ScenarioPress_Event>;
using ScenarioMove_Transition = MatchedTransition<ScenarioMove_Event>;
using ScenarioRelease_Transition = MatchedTransition<ScenarioRelease_Event>;
