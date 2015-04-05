#pragma once
#include <QStateMachine>
#include <QState>
#include <QAbstractTransition>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>

// A coordinate : (t, y)
struct ScenarioPoint
{
        TimeValue date;
        double y;
};

class CommonState : public QState
{
    public:
        CommonState(ObjectPath&& scenar, QState* parent):
            QState{parent},
            m_scenarioPath{std::move(scenar)}
        { }

        void clear()
        {
            // TODO faire un StateData qu'on peut operator='er à la place ?
            // Vérifier que c'est nécessaire dans tous les cas.
            clickedEvent = id_type<EventModel>{};
            clickedTimeNode = id_type<TimeNodeModel>{};
            clickedConstraint = id_type<ConstraintModel>{};

            date = TimeValue{};
            ypos = 0;

        }

        id_type<EventModel> clickedEvent;
        id_type<TimeNodeModel> clickedTimeNode;
        id_type<ConstraintModel> clickedConstraint;

        id_type<EventModel> hoveredEvent;
        id_type<TimeNodeModel> hoveredTimeNode;
        id_type<ConstraintModel> hoveredConstraint;

        TimeValue date;
        double ypos{};

        const auto& createdEvent() const
        { return m_createdEvent; }
        const auto& createdTimeNode() const
        { return m_createdTimeNode; }

    protected:
        ObjectPath m_scenarioPath;

        id_type<EventModel> m_createdEvent;
        id_type<TimeNodeModel> m_createdTimeNode;
};

////////////////////////

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
    protected:
        virtual bool eventTest(QEvent *e) override
        { return e->type() == QEvent::Type(QEvent::User + Event::user_type); }

        virtual void onTransition(QEvent *event) override { }
};

using Cancel_Event = NumberedEvent<100>;
using Cancel_Transition = MatchedTransition<Cancel_Event>;

template<typename Event>
class AbstractScenarioTransition : public MatchedTransition<Event>
{
    public:
        AbstractScenarioTransition(CommonState& state):
                    m_state{state} { }

        CommonState& state() const { return m_state; }

    private:
        CommonState& m_state;
};




////////////

template<int N>
struct Positioned_Event : public NumberedEvent<N>
{
        Positioned_Event(const TimeValue& newdate, double newy):
            date{newdate},
            y{newy}
        {
        }

        Positioned_Event(const ScenarioPoint& pt):
            date{pt.date},
            y{pt.y}
        {
        }

        TimeValue date;
        double y{};
};
template<typename Element, int N>
struct PositionedOn_Event : public Positioned_Event<N>
{
        PositionedOn_Event(const id_type<Element>& tn_id,
                           const TimeValue& newdate,
                           double newy):
            Positioned_Event<N>{newdate, newy},
            id{tn_id}
        {
        }

        id_type<Element> id;
};

// Specialized
using ClickOnNothing_Event = Positioned_Event<1>;
using ClickOnTimeNode_Event = PositionedOn_Event<TimeNodeModel, 2>;
using ClickOnEvent_Event = PositionedOn_Event<EventModel, 3>;
using ClickOnConstraint_Event = PositionedOn_Event<ConstraintModel, 4>;

using MoveOnNothing_Event = Positioned_Event<5>;
using MoveOnTimeNode_Event = PositionedOn_Event<TimeNodeModel, 6>;
using MoveOnEvent_Event = PositionedOn_Event<EventModel, 7>;
using MoveOnConstraint_Event = PositionedOn_Event<ConstraintModel, 8>;

using ReleaseOnNothing_Event = Positioned_Event<9>;
using ReleaseOnTimeNode_Event = PositionedOn_Event<TimeNodeModel, 10>;
using ReleaseOnEvent_Event = PositionedOn_Event<EventModel, 11>;
using ReleaseOnConstraint_Event = PositionedOn_Event<ConstraintModel, 12>;

// Not specialized
using ScenarioPress_Event = Positioned_Event<13>;
using ScenarioMove_Event = Positioned_Event<14>;
using ScenarioRelease_Event = Positioned_Event<15>;

///////////
class ClickOnNothing_Transition : public AbstractScenarioTransition<ClickOnNothing_Event>
{
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<ClickOnNothing_Event*>(ev);
            this->state().clear();

            // TODO this should instead be set within the state ?
            this->state().clickedEvent = id_type<EventModel>(0);
            this->state().date = qev->date;
            this->state().ypos = qev->y;
        }
};

class ClickOnTimeNode_Transition : public AbstractScenarioTransition<ClickOnTimeNode_Event>
{
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<ClickOnTimeNode_Event*>(ev);
            this->state().clear();

            this->state().clickedTimeNode = qev->id;
            this->state().date = qev->date;
            this->state().ypos = qev->y;
        }
};

class ClickOnEvent_Transition : public AbstractScenarioTransition<ClickOnEvent_Event>
{
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<ClickOnEvent_Event*>(ev);
            this->state().clear();

            this->state().clickedEvent = qev->id;
            this->state().date = qev->date;
            this->state().ypos = qev->y;
        }
};

class ClickOnConstraint_Transition : public AbstractScenarioTransition<ClickOnConstraint_Event>
{
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<ClickOnConstraint_Event*>(ev);
            this->state().clear();

            this->state().clickedConstraint = qev->id;
            this->state().date = qev->date;
            this->state().ypos = qev->y;
        }
};

////////////////////////
class MoveOnNothing_Transition : public AbstractScenarioTransition<MoveOnNothing_Event>
{
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<MoveOnNothing_Event*>(ev);

            this->state().date = qev->date;
            this->state().ypos =  qev->y;
        }
};

class MoveOnTimeNode_Transition : public AbstractScenarioTransition<MoveOnTimeNode_Event>
{
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<MoveOnTimeNode_Event*>(ev);

            this->state().hoveredTimeNode = qev->id;
            this->state().date = qev->date;
            this->state().ypos = qev->y;
        }
};

class MoveOnEvent_Transition : public AbstractScenarioTransition<MoveOnEvent_Event>
{
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<MoveOnEvent_Event*>(ev);

            this->state().hoveredEvent = qev->id;
            this->state().date = qev->date;
            this->state().ypos = qev->y;
        }
};


////////////////////////
class ReleaseOnNothing_Transition : public AbstractScenarioTransition<ReleaseOnNothing_Event>
{
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<ReleaseOnNothing_Event*>(ev);

            this->state().date = qev->date;
            this->state().ypos =  qev->y;
        }
};

class ReleaseOnTimeNode_Transition : public AbstractScenarioTransition<ReleaseOnTimeNode_Event>
{
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<ReleaseOnTimeNode_Event*>(ev);

            this->state().hoveredTimeNode = qev->id;
            this->state().date = qev->date;
            this->state().ypos = qev->y;
        }
};

class ReleaseOnEvent_Transition : public AbstractScenarioTransition<ReleaseOnEvent_Event>
{
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<ReleaseOnEvent_Event*>(ev);

            this->state().hoveredEvent = qev->id;
            this->state().date = qev->date;
            this->state().ypos = qev->y;
        }
};

class ReleaseOnAnything_Transition : public QAbstractTransition
{
    protected:
        bool eventTest(QEvent *e) override
        {
            using namespace std;
            static const constexpr int types[] = {
                QEvent::User + ReleaseOnNothing_Event::user_type,
                QEvent::User + ReleaseOnEvent_Event::user_type,
                QEvent::User + ReleaseOnTimeNode_Event::user_type,
                QEvent::User + ReleaseOnConstraint_Event::user_type};

            return find(begin(types), end(types), e->type()) != end(types);
        }

        void onTransition(QEvent *event) override { }
};
