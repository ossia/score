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
class CreateEventState : public QState
{
        Q_OBJECT
    public:
        CreateEventState(ObjectPath&& scenarioPath,
                         iscore::CommandStack& stack,
                         QState* parent);

        id_type<EventModel> firstEvent;
        TimeValue eventDate;
        double ypos{};

        const auto& createdEvent() const
        { return m_createdEvent; }
        const auto& createdTimeNode() const
        { return m_createTimeNode; }

    private:
        ObjectPath m_scenarioPath;
        RealtimeMacroCommandDispatcher m_dispatcher;
        id_type<EventModel> m_createdEvent;
        id_type<TimeNodeModel> m_createTimeNode;
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
    public:
        using QAbstractTransition::QAbstractTransition;

    protected:
        virtual bool eventTest(QEvent *e)
        { return e->type() == QEvent::Type(QEvent::User+1000); }

        virtual void onTransition(QEvent * ev)
        { }
};


class AbstractScenarioTransition : public QAbstractTransition
{
    public:
        AbstractScenarioTransition(CreateEventState* state):
                    m_state{state} { }

        CreateEventState& state() const { return *m_state; }

    private:
        CreateEventState* m_state{};
};


////////////
template<typename Element, int N>
struct QEvent_ClickOn_T : public QEvent
{
        static constexpr const int user_type = N;
        QEvent_ClickOn_T(const id_type<Element>& tn_id,
                               const TimeValue& newdate,
                               double newy):
            QEvent{QEvent::Type(QEvent::User + N)},
            id{tn_id},
            date{newdate},
            y{newy}
        {
        }

        id_type<Element> id;
        TimeValue date;
        double y{};
};

struct ClickOnNothing_Event : public QEvent
{
        ClickOnNothing_Event(const TimeValue& newdate, double newy):
            QEvent{QEvent::Type(QEvent::User+1)},
            date{newdate},
            y{newy}
        {
        }

        TimeValue date;
        double y{};
};


class ClickOnNothing_Transition : public AbstractScenarioTransition
{
        Q_OBJECT
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual bool eventTest(QEvent *e)
        { return e->type() == QEvent::Type(QEvent::User+1); }

        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<ClickOnNothing_Event*>(ev);

            state().firstEvent = id_type<EventModel>(0);
            state().eventDate = qev->date;
            state().ypos =  qev->y;
        }
};


////////////////////////
using ClickOnTimeNode_Event = QEvent_ClickOn_T<TimeNodeModel, 2>;
class ClickOnTimeNode_Transition : public AbstractScenarioTransition
{
        Q_OBJECT
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual bool eventTest(QEvent *e)
        { return e->type() == QEvent::Type(QEvent::User + ClickOnTimeNode_Event::user_type); }

        virtual void onTransition(QEvent * ev)
        {
            //auto qev = static_cast<ScenarioClickOnTimeNode_QEvent*>(ev);
            //state().press(qev->m_id, qev->m_date, qev->m_ypos);
        }
};


////////////////////////
using ClickOnEvent_Event = QEvent_ClickOn_T<EventModel, 3>;
class ClickOnEvent_Transition : public AbstractScenarioTransition
{
        Q_OBJECT
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual bool eventTest(QEvent *e)
        { return e->type() == QEvent::Type(QEvent::User +  + ClickOnEvent_Event::user_type); }

        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<ClickOnEvent_Event*>(ev);

            state().firstEvent = qev->id;
            state().eventDate = qev->date;
            state().ypos =  qev->y;
        }
};

////////////////
using ClickOnConstraint_Event = QEvent_ClickOn_T<ConstraintModel, 4>;
class ClickOnConstraint_Transition : public AbstractScenarioTransition
{
        Q_OBJECT
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual bool eventTest(QEvent *e)
        { return e->type() == QEvent::Type(QEvent::User + ClickOnConstraint_Event::user_type); }

        virtual void onTransition(QEvent * ev)
        {
            //auto qev = static_cast<ScenarioClickOnTimeNode_QEvent*>(ev);
            //state().press(qev->m_id, qev->m_date, qev->m_ypos);
        }
};
////////////////////////
struct Move_Event : public QEvent
{
        Move_Event(const TimeValue& newdate, double newy):
            QEvent{QEvent::Type(QEvent::User+5)},
            m_date{newdate},
            m_ypos{newy}
        {
        }

        TimeValue m_date;
        double m_ypos{};
};
class Move_Transition : public AbstractScenarioTransition
{
        Q_OBJECT
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual bool eventTest(QEvent *e)
        { return e->type() == QEvent::Type(QEvent::User+5); }

        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<Move_Event*>(ev);

            state().eventDate = qev->m_date;
            state().ypos =  qev->m_ypos;
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
