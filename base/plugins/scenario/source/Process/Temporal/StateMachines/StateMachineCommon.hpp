#pragma once
#include "StateMachineVeryCommon.hpp"
#include <QStateMachine>
#include <QState>
#include <QAbstractTransition>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>

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

            point = ScenarioPoint{};

        }

        id_type<EventModel> clickedEvent;
        id_type<TimeNodeModel> clickedTimeNode;
        id_type<ConstraintModel> clickedConstraint;

        id_type<EventModel> hoveredEvent;
        id_type<TimeNodeModel> hoveredTimeNode;
        id_type<ConstraintModel> hoveredConstraint;

        ScenarioPoint point;

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
template<typename Element, int N>
struct PositionedOn_Event : public PositionedEvent<N>
{
        PositionedOn_Event(const id_type<Element>& tn_id,
                           const ScenarioPoint& sp):
            PositionedEvent<N>{sp},
            id{tn_id}
        {
        }

        id_type<Element> id;
};

// Events
using Cancel_Event = NumberedEvent<10>;
using ClickOnNothing_Event = PositionedEvent<11>;
using ClickOnTimeNode_Event = PositionedOn_Event<TimeNodeModel, 12>;
using ClickOnEvent_Event = PositionedOn_Event<EventModel, 13>;
using ClickOnConstraint_Event = PositionedOn_Event<ConstraintModel, 14>;

using MoveOnNothing_Event = PositionedEvent<15>;
using MoveOnTimeNode_Event = PositionedOn_Event<TimeNodeModel, 16>;
using MoveOnEvent_Event = PositionedOn_Event<EventModel, 17>;
using MoveOnConstraint_Event = PositionedOn_Event<ConstraintModel, 18>;

using ReleaseOnNothing_Event = PositionedEvent<19>;
using ReleaseOnTimeNode_Event = PositionedOn_Event<TimeNodeModel, 20>;
using ReleaseOnEvent_Event = PositionedOn_Event<EventModel, 21>;
using ReleaseOnConstraint_Event = PositionedOn_Event<ConstraintModel, 22>;

// Transitions

using Cancel_Transition = MatchedTransition<Cancel_Event>;

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
            this->state().point = qev->point;
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
            this->state().point = qev->point;
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
            this->state().point = qev->point;
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
            this->state().point = qev->point;
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

            this->state().point = qev->point;
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
            this->state().point = qev->point;
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
            this->state().point = qev->point;
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

            this->state().point = qev->point;
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
            this->state().point = qev->point;
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
            this->state().point = qev->point;
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
