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

#include <Document/Event/EventData.hpp>
class CreateEventStateMachine : public QObject
{
        Q_OBJECT
    public:
        CreateEventStateMachine(iscore::CommandStack& stack);

        void init(ObjectPath&& path,
                  id_type<EventModel> startEvent,
                  const TimeValue& date,
                  double y);

        void move(const TimeValue& newdate, double newy)
        {
            m_eventDate = newdate;
            m_ypos = newy;
            emit move();
        }

    signals:
        void move();
        void release();
        void cancel();

    private:
        QStateMachine m_sm;
        RealtimeMacroCommandDispatcher m_dispatcher;

        ObjectPath m_scenarioPath;

        id_type<EventModel> m_firstEvent{0};
        id_type<EventModel> m_createdEvent;
        TimeValue m_eventDate;
        double m_ypos{};
};
