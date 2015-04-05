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
struct Positioned_Event : public NumberedEvent<N>
{
        Positioned_Event(const TimeValue& newdate, double newy):
            point{newdate, newy}
        {
        }

        Positioned_Event(const ScenarioPoint& pt):
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
using ScenarioPress_Event = Positioned_Event<1>;
using ScenarioMove_Event = Positioned_Event<2>;
using ScenarioRelease_Event = Positioned_Event<3>;
