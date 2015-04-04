#pragma once
#include <QStateMachine>
#include <QState>


#include <iscore/command/OngoingCommandManager.hpp>

#include <Commands/Scenario/Creations/CreateEvent.hpp>
#include <Commands/Scenario/Displacement/MoveEvent.hpp>

class CreateEventAggregate : public iscore::AggregateCommand
{
    public:
        CreateEventAggregate(iscore::SerializableCommand* creation, iscore::SerializableCommand* move):
            iscore::AggregateCommand{"ScenarioControl",
                                     "CreateEventAggregate",
                                     "CreateEventAggregate_desc",
                                     creation, move}
        {

        }
};

class RealtimeMacroCommandDispatcher : public ITransactionalCommandDispatcher
{

    public:
        template<typename... Args>
        RealtimeMacroCommandDispatcher(Args&&... args):
            ITransactionalCommandDispatcher{std::forward<Args&&>(args)...}
        {
            connect(this, &RealtimeMacroCommandDispatcher::submitCommand,
                    this, &RealtimeMacroCommandDispatcher::send_impl,
                    Qt::DirectConnection);

            connect(this, &RealtimeMacroCommandDispatcher::commit,
                    this, &RealtimeMacroCommandDispatcher::commit_impl,
                    Qt::DirectConnection);
        }

    private:
        void send_impl(iscore::SerializableCommand* cmd)
        {
            if(!m_base)
            {
                m_base = cmd;
                m_base->redo();
            }
            else
            {
                if(!m_continuous)
                {
                    m_continuous = cmd;
                    m_continuous->redo();
                }
                else
                {
                    MergeStrategy::Simple::merge(m_continuous, cmd);
                }
            }
        }

        void commit_impl()
        {
            if(!m_continuous)
            {
                // Only send the base command
                SendStrategy::Quiet::send(stack(), m_base);
            }
            else
            {
                // Send an Aggregate made with the two commands.
                auto cmd = new CreateEventAggregate{m_base, m_continuous};
                SendStrategy::Quiet::send(stack(), cmd);
            }

            m_base = nullptr;
            m_continuous = nullptr;
        }

        iscore::SerializableCommand* m_base{};
        iscore::SerializableCommand* m_continuous{};
};




#include <QAbstractTransition>
#include <Document/Event/EventData.hpp>
class CommonState : public QState
{
    public:
        using QState::QState;
        id_type<EventModel> firstEvent;
        TimeValue eventDate;
        double ypos{};

        const auto& createdEvent() const
        { return m_createdEvent; }
        const auto& createdTimeNode() const
        { return m_createTimeNode; }

    protected:
        id_type<EventModel> m_createdEvent;
        id_type<TimeNodeModel> m_createTimeNode;
};

class CreateEventState : public CommonState
{
        Q_OBJECT
    public:
        CreateEventState(ObjectPath&& scenarioPath,
                         iscore::CommandStack& stack,
                         QState* parent);


    private:
        ObjectPath m_scenarioPath;
        RealtimeMacroCommandDispatcher m_dispatcher;
};





////////////////////////
struct Cancel_Event : public QEvent
{
        Cancel_Event():
            QEvent{QEvent::Type(QEvent::User+100)}
        { }
};

class Cancel_Transition : public QAbstractTransition
{
        Q_OBJECT
    public:
        using QAbstractTransition::QAbstractTransition;

    protected:
        virtual bool eventTest(QEvent *e)
        { return e->type() == QEvent::Type(QEvent::User+100); }

        virtual void onTransition(QEvent * ev)
        { }
};


struct Release_Event : public QEvent
{
        Release_Event():
            QEvent{QEvent::Type(QEvent::User+1000)}
        { }
};

class Release_Transition : public QAbstractTransition
{
        Q_OBJECT
    protected:
        virtual bool eventTest(QEvent *e)
        { return e->type() == QEvent::Type(QEvent::User+1000); }

        virtual void onTransition(QEvent * ev)
        { }
};


template<typename Event>
class MatchedTransition : public QAbstractTransition
{
    protected:
        virtual bool eventTest(QEvent *e)
        { return e->type() == QEvent::Type(QEvent::User + Event::user_type); }
};

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
struct Positioned_Event : public QEvent
{
        static constexpr const int user_type = N;
        Positioned_Event(const TimeValue& newdate, double newy):
            QEvent{QEvent::Type(QEvent::User + N)},
            date{newdate},
            y{newy}
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

///////////
class ClickOnNothing_Transition : public AbstractScenarioTransition<ClickOnNothing_Event>
{
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<ClickOnNothing_Event*>(ev);

            // TODO this should instead be set within the state ?
            this->state().firstEvent = id_type<EventModel>(0);
            this->state().eventDate = qev->date;
            this->state().ypos =  qev->y;
        }
};


////////////////////////
class ClickOnTimeNode_Transition : public AbstractScenarioTransition<ClickOnTimeNode_Event>
{
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            //auto qev = static_cast<ScenarioClickOnTimeNode_QEvent*>(ev);
            //state().press(qev->m_id, qev->m_date, qev->m_ypos);
        }
};


////////////////////////
class ClickOnEvent_Transition : public AbstractScenarioTransition<ClickOnEvent_Event>
{
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<ClickOnEvent_Event*>(ev);

            this->state().firstEvent = qev->id;
            this->state().eventDate = qev->date;
            this->state().ypos =  qev->y;
        }
};

////////////////
class ClickOnConstraint_Transition : public AbstractScenarioTransition<ClickOnConstraint_Event>
{
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            //auto qev = static_cast<ScenarioClickOnTimeNode_QEvent*>(ev);
            //state().press(qev->m_id, qev->m_date, qev->m_ypos);
        }
};

////////////////////////
class Move_Transition : public AbstractScenarioTransition<MoveOnNothing_Event>
{
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<MoveOnNothing_Event*>(ev);

            this->state().eventDate = qev->date;
            this->state().ypos =  qev->y;
        }
};



struct HoverEvent_Event : public QEvent
{
        HoverEvent_Event(const EventModel& ev):
            QEvent{QEvent::Type(QEvent::User+50)},
            m_event{ev}
        {
        }

        const EventModel& m_event;
};

struct HoverTimeNode_Event : public QEvent
{
        HoverTimeNode_Event(const TimeNodeModel& tn):
            QEvent{QEvent::Type(QEvent::User+51)},
            m_timenode{tn}
        {
        }

        const TimeNodeModel& m_timenode;
};
