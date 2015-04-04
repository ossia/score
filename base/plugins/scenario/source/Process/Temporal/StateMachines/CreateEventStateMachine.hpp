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
struct ScenarioCancel_QEvent : public QEvent
{
        ScenarioCancel_QEvent():
            QEvent{QEvent::Type(QEvent::User+100)}
        { }
};
class ScenarioCancelTransition : public QAbstractTransition
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


struct ScenarioRelease_QEvent : public QEvent
{
        ScenarioRelease_QEvent():
            QEvent{QEvent::Type(QEvent::User+10)}
        { }
};

class ScenarioRelease_Transition : public QAbstractTransition
{
        Q_OBJECT
    public:
        using QAbstractTransition::QAbstractTransition;

    protected:
        virtual bool eventTest(QEvent *e)
        { return e->type() == QEvent::Type(QEvent::User+10); }

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
struct ScenarioClickOnNothing_QEvent : public QEvent
{
        ScenarioClickOnNothing_QEvent(const TimeValue& newdate, double newy):
            QEvent{QEvent::Type(QEvent::User+1)},
            m_eventDate{newdate},
            m_ypos{newy}
        {
        }

        TimeValue m_eventDate;
        double m_ypos{};
};

class ScenarioClickOnNothing_Transition : public AbstractScenarioTransition
{
        Q_OBJECT
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual bool eventTest(QEvent *e)
        { return e->type() == QEvent::Type(QEvent::User+1); }

        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<ScenarioClickOnNothing_QEvent*>(ev);

            state().firstEvent = id_type<EventModel>(0);
            state().eventDate = qev->m_eventDate;
            state().ypos =  qev->m_ypos;
        }
};


////////////////////////
struct ScenarioClickOnTimeNode_QEvent : public QEvent
{
        ScenarioClickOnTimeNode_QEvent(const id_type<TimeNodeModel>& tn_id,
                                       const TimeValue& newdate,
                                       double newy):
            QEvent{QEvent::Type(QEvent::User+2)},
            m_id{tn_id},
            m_date{newdate},
            m_ypos{newy}
        {
        }

        id_type<TimeNodeModel> m_id;
        TimeValue m_date;
        double m_ypos{};
};

class ScenarioClickOnTimeNode_Transition : public AbstractScenarioTransition
{
        Q_OBJECT
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual bool eventTest(QEvent *e)
        { return e->type() == QEvent::Type(QEvent::User+2); }

        virtual void onTransition(QEvent * ev)
        {
            //auto qev = static_cast<ScenarioClickOnTimeNode_QEvent*>(ev);
            //state().press(qev->m_id, qev->m_date, qev->m_ypos);
        }
};


////////////////////////
struct ScenarioClickOnEvent_QEvent : public QEvent
{
        ScenarioClickOnEvent_QEvent(id_type<EventModel> e, const TimeValue& newdate, double newy):
            QEvent{QEvent::Type(QEvent::User+3)},
            m_id{e},
            m_date{newdate},
            m_ypos{newy}
        {
        }

        id_type<EventModel> m_id;
        TimeValue m_date;
        double m_ypos{};
};

class ScenarioClickOnEvent_Transition : public AbstractScenarioTransition
{
        Q_OBJECT
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual bool eventTest(QEvent *e)
        { return e->type() == QEvent::Type(QEvent::User+3); }

        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<ScenarioClickOnEvent_QEvent*>(ev);

            state().firstEvent = qev->m_id;
            state().eventDate = qev->m_date;
            state().ypos =  qev->m_ypos;
        }
};

////////////////
struct ScenarioClickOnConstraint_QEvent : public QEvent
{
        ScenarioClickOnConstraint_QEvent(const id_type<ConstraintModel>& tn_id,
                                         const TimeValue& newdate,
                                         double newy):
            QEvent{QEvent::Type(QEvent::User+5)},
            m_id{tn_id},
            m_date{newdate},
            m_ypos{newy}
        {
        }

        id_type<ConstraintModel> m_id;
        TimeValue m_date;
        double m_ypos{};
};

class ScenarioClickOnConstraint_Transition : public AbstractScenarioTransition
{
        Q_OBJECT
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual bool eventTest(QEvent *e)
        { return e->type() == QEvent::Type(QEvent::User+5); }

        virtual void onTransition(QEvent * ev)
        {
            //auto qev = static_cast<ScenarioClickOnTimeNode_QEvent*>(ev);
            //state().press(qev->m_id, qev->m_date, qev->m_ypos);
        }
};
////////////////////////
struct ScenarioMoveOverNothing_QEvent : public QEvent
{
        ScenarioMoveOverNothing_QEvent(const TimeValue& newdate, double newy):
            QEvent{QEvent::Type(QEvent::User+4)},
            m_date{newdate},
            m_ypos{newy}
        {
        }

        TimeValue m_date;
        double m_ypos{};
};
class ScenarioMove_Transition : public AbstractScenarioTransition
{
        Q_OBJECT
    public:
        using AbstractScenarioTransition::AbstractScenarioTransition;

    protected:
        virtual bool eventTest(QEvent *e)
        { return e->type() == QEvent::Type(QEvent::User+4); }

        virtual void onTransition(QEvent * ev)
        {
            auto qev = static_cast<ScenarioMoveOverNothing_QEvent*>(ev);

            state().eventDate = qev->m_date;
            state().ypos =  qev->m_ypos;
        }
};



struct ScenarioHoverEvent_QEvent : public QEvent
{
        ScenarioHoverEvent_QEvent(const EventModel& ev):
            QEvent{QEvent::Type(QEvent::User+50)},
            m_event{ev}
        {
        }

        const EventModel& m_event;
};

struct ScenarioHoverTimeNode_QEvent : public QEvent
{
        ScenarioHoverTimeNode_QEvent(const TimeNodeModel& tn):
            QEvent{QEvent::Type(QEvent::User+51)},
            m_timenode{tn}
        {
        }

        const TimeNodeModel& m_timenode;
};
