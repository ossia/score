#pragma once
#include "StateMachineVeryCommon.hpp"
#include <QStateMachine>
#include <QState>
#include <QAbstractTransition>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Constraint/Box/Deck/DeckModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>



// TODO optimize this when we have all the tools
class CommonScenarioState : public QState
{
    public:
        CommonScenarioState(ObjectPath&& scenar, QState* parent):
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

            currentPoint = ScenarioPoint();
        }

        id_type<EventModel> clickedEvent;
        id_type<TimeNodeModel> clickedTimeNode;
        id_type<ConstraintModel> clickedConstraint;

        id_type<EventModel> hoveredEvent;
        id_type<TimeNodeModel> hoveredTimeNode;
        id_type<ConstraintModel> hoveredConstraint;

        ScenarioPoint currentPoint;

    protected:
        ObjectPath m_scenarioPath;
};

class CreationState : public CommonScenarioState
{
    public:
        using CommonScenarioState::CommonScenarioState;
        const auto& createdEvent() const
        { return m_createdEvent; }
        void setCreatedEvent(const id_type<EventModel>& id)
        { m_createdEvent = id; }

        const auto& createdTimeNode() const
        { return m_createdTimeNode; }
        void setCreatedTimeNode(const id_type<TimeNodeModel>& id)
        { m_createdTimeNode = id; }

    private:
        id_type<EventModel> m_createdEvent;
        id_type<TimeNodeModel> m_createdTimeNode;
};

////////////////////////

// TODO rename in something like CreateMove transition...
// Or something that means that it will operate on constraint, event, etc...
template<typename T>
class GenericTransition : public T
{
    public:
        GenericTransition(CommonScenarioState& state):
                    m_state{state} { }

        CommonScenarioState& state() const { return m_state; }

    private:
        CommonScenarioState& m_state;
};

template<typename Event>
class MatchedScenarioTransition : public GenericTransition<MatchedTransition<Event>>
{
    public:
        using GenericTransition<MatchedTransition<Event>>::GenericTransition;
};




////////////
template<typename Element, int N>
struct PositionedWithId_Event : public PositionedEvent<N>
{
        PositionedWithId_Event(const id_type<Element>& tn_id,
                           const ScenarioPoint& sp):
            PositionedEvent<N>{sp},
            id{tn_id}
        {
        }

        id_type<Element> id;
};


template<typename Element, int N>
struct PositionedWithPath_Event : public PositionedEvent<N>
{
        PositionedWithPath_Event(const ObjectPath& p,
                                 const ScenarioPoint& sp):
            PositionedEvent<N>{sp},
            path{p}
        {
        }

        ObjectPath path;
};

// Events
enum ScenarioElement {
    Nothing, TimeNode, Event, Constraint, Deck, DeckHandle
};
enum Modifier
{ Click = 100, Move = 200, Release = 300 };

using ClickOnNothing_Event = PositionedEvent<ScenarioElement::Nothing + Modifier::Click>;
using ClickOnTimeNode_Event = PositionedWithId_Event<TimeNodeModel, ScenarioElement::TimeNode + Modifier::Click>;
using ClickOnEvent_Event = PositionedWithId_Event<EventModel, ScenarioElement::Event + Modifier::Click>;
using ClickOnConstraint_Event = PositionedWithId_Event<ConstraintModel, ScenarioElement::Constraint + Modifier::Click>;

using ClickOnDeck_Event = PositionedWithPath_Event<DeckModel, ScenarioElement::Deck + Modifier::Click>;
using ClickOnDeckHandle_Event = PositionedWithPath_Event<DeckModel, ScenarioElement::DeckHandle + Modifier::Click>;


using MoveOnNothing_Event = PositionedEvent<ScenarioElement::Nothing + Modifier::Move>;
using MoveOnTimeNode_Event = PositionedWithId_Event<TimeNodeModel, ScenarioElement::TimeNode + Modifier::Move>;
using MoveOnEvent_Event = PositionedWithId_Event<EventModel, ScenarioElement::Event + Modifier::Move>;
using MoveOnConstraint_Event = PositionedWithId_Event<ConstraintModel, ScenarioElement::Constraint + Modifier::Move>;

using MoveOnDeck_Event = PositionedWithPath_Event<DeckModel, ScenarioElement::Deck + Modifier::Move>;
using MoveOnDeckHandle_Event = PositionedWithPath_Event<DeckModel, ScenarioElement::DeckHandle + Modifier::Move>;


using ReleaseOnNothing_Event = PositionedEvent<ScenarioElement::Nothing + Modifier::Release>;
using ReleaseOnTimeNode_Event = PositionedWithId_Event<TimeNodeModel, ScenarioElement::TimeNode + Modifier::Release>;
using ReleaseOnEvent_Event = PositionedWithId_Event<EventModel, ScenarioElement::Event + Modifier::Release>;
using ReleaseOnConstraint_Event = PositionedWithId_Event<ConstraintModel, ScenarioElement::Constraint + Modifier::Release>;

using ReleaseOnDeck_Event = PositionedWithPath_Event<DeckModel, ScenarioElement::Deck + Modifier::Release>;
using ReleaseOnDeckHandle_Event = PositionedWithPath_Event<DeckModel, ScenarioElement::DeckHandle + Modifier::Release>;

// Transitions

///////////
class ClickOnNothing_Transition : public MatchedScenarioTransition<ClickOnNothing_Event>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<ClickOnNothing_Event*>(ev);
            this->state().clear();

            this->state().currentPoint = qev->point;
        }
};

class ClickOnTimeNode_Transition : public MatchedScenarioTransition<ClickOnTimeNode_Event>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<ClickOnTimeNode_Event*>(ev);
            this->state().clear();

            this->state().clickedTimeNode = qev->id;
            this->state().currentPoint = qev->point;
        }
};

class ClickOnEvent_Transition : public MatchedScenarioTransition<ClickOnEvent_Event>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<ClickOnEvent_Event*>(ev);
            this->state().clear();

            this->state().clickedEvent = qev->id;
            this->state().currentPoint = qev->point;
        }
};

class ClickOnConstraint_Transition : public MatchedScenarioTransition<ClickOnConstraint_Event>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<ClickOnConstraint_Event*>(ev);
            this->state().clear();

            this->state().clickedConstraint = qev->id;
            this->state().currentPoint = qev->point;
        }
};




class DeckState : public QState
{
    public:
        DeckState(QState* parent):
            QState{parent}
        { }



        ObjectPath currentDeck;
        ScenarioPoint currentPoint;
};

class ClickOnDeck_Transition : public MatchedTransition<ClickOnDeck_Event>
{
    public:
        ClickOnDeck_Transition(DeckState& state):
            m_state{state}
        {

        }

        DeckState& state() const { return m_state; }

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<event_type*>(ev);

            this->state().currentDeck = qev->path;
            this->state().currentPoint = qev->point;
        }

    private:
        DeckState& m_state;
};

class ClickOnDeckHandle_Transition : public MatchedTransition<ClickOnDeckHandle_Event>
{
    public:
        ClickOnDeckHandle_Transition(DeckState& state):
            m_state{state}
        {

        }

        DeckState& state() const { return m_state; }

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<event_type*>(ev);

            this->state().currentDeck = qev->path;
            this->state().currentPoint = qev->point;
        }

    private:
        DeckState& m_state;
};


////////////////////////
class MoveOnNothing_Transition : public MatchedScenarioTransition<MoveOnNothing_Event>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<MoveOnNothing_Event*>(ev);

            this->state().currentPoint = qev->point;
        }
};

class MoveOnTimeNode_Transition : public MatchedScenarioTransition<MoveOnTimeNode_Event>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<MoveOnTimeNode_Event*>(ev);

            this->state().hoveredTimeNode = qev->id;
            this->state().currentPoint = qev->point;
        }
};

class MoveOnEvent_Transition : public MatchedScenarioTransition<MoveOnEvent_Event>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<MoveOnEvent_Event*>(ev);

            this->state().hoveredEvent = qev->id;
            this->state().currentPoint = qev->point;
        }
};

class MoveOnConstraint_Transition : public MatchedScenarioTransition<MoveOnConstraint_Event>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<MoveOnConstraint_Event*>(ev);

            this->state().hoveredConstraint= qev->id;
            this->state().currentPoint = qev->point;
        }
};


class MoveOnAnything_Transition : public GenericTransition<QAbstractTransition>
{
    public:
        using GenericTransition<QAbstractTransition>::GenericTransition;

    protected:
        bool eventTest(QEvent *e) override
        {
            using namespace std;
            static const constexpr QEvent::Type types[] = {
                QEvent::Type(QEvent::User + MoveOnNothing_Event::user_type),
                QEvent::Type(QEvent::User + MoveOnEvent_Event::user_type),
                QEvent::Type(QEvent::User + MoveOnTimeNode_Event::user_type),
                QEvent::Type(QEvent::User + MoveOnConstraint_Event::user_type)};

            return find(begin(types), end(types), e->type()) != end(types);
        }

        void onTransition(QEvent *event) override
        {
            auto qev = static_cast<PositionedEventBase*>(event);

            this->state().currentPoint = qev->point;
        }
};

////////////////////////
class ReleaseOnNothing_Transition : public MatchedScenarioTransition<ReleaseOnNothing_Event>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<ReleaseOnNothing_Event*>(ev);

            this->state().currentPoint = qev->point;
        }
};

class ReleaseOnTimeNode_Transition : public MatchedScenarioTransition<ReleaseOnTimeNode_Event>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<ReleaseOnTimeNode_Event*>(ev);

            this->state().hoveredTimeNode = qev->id;
            this->state().currentPoint = qev->point;
        }
};

class ReleaseOnEvent_Transition : public MatchedScenarioTransition<ReleaseOnEvent_Event>
{
    public:
        using MatchedScenarioTransition::MatchedScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<ReleaseOnEvent_Event*>(ev);

            this->state().hoveredEvent = qev->id;
            this->state().currentPoint = qev->point;
        }
};

class ReleaseOnAnything_Transition : public QAbstractTransition
{
    protected:
        bool eventTest(QEvent *e) override
        {
            using namespace std;
            static const constexpr QEvent::Type types[] = {
                QEvent::Type(QEvent::User + ReleaseOnNothing_Event::user_type),
                QEvent::Type(QEvent::User + ReleaseOnEvent_Event::user_type),
                QEvent::Type(QEvent::User + ReleaseOnTimeNode_Event::user_type),
                QEvent::Type(QEvent::User + ReleaseOnConstraint_Event::user_type)};

            return find(begin(types), end(types), e->type()) != end(types);
        }

        void onTransition(QEvent *event) override { }
};
