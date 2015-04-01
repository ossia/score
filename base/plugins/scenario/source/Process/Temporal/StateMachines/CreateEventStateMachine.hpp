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
                m_base = nullptr;
            }
            else
            {
                // Send an Aggregate made with the two commands.
                auto cmd = new CreateEventAggregate{m_base, m_continuous};
                SendStrategy::Quiet::send(stack(), cmd);
            }
        }

        iscore::SerializableCommand* m_base{};
        iscore::SerializableCommand* m_continuous{};
};

class CreateEventStateMachine : public QObject
{
        Q_OBJECT
    public:
        CreateEventStateMachine(iscore::CommandStack& stack);

        void move(const QPointF& newpos)
        {
            m_pos = newpos;
            emit move();
        }

    signals:
        void move();
        void release();
        void cancel();

    private:
        QPointF m_pos;
        QStateMachine m_sm;
        RealtimeMacroCommandDispatcher m_dispatcher;
};
