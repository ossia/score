#pragma once
#include <iscore/command/OngoingCommandManager.hpp>
#include "Process/Temporal/StateMachines/StateMachineCommon.hpp"

class MetaCreateEvent : public iscore::AggregateCommand
{
    public:
        MetaCreateEvent(iscore::SerializableCommand* creation, iscore::SerializableCommand* move):
            iscore::AggregateCommand{"ScenarioControl",
                                     "MetaCreateEvent",
                                     "MetaCreateEvent_desc",
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

            connect(this, &RealtimeMacroCommandDispatcher::rollback,
                    this, &RealtimeMacroCommandDispatcher::rollback_impl,
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
                auto cmd = new MetaCreateEvent{m_base, m_continuous};
                SendStrategy::Quiet::send(stack(), cmd);
            }

            m_base = nullptr;
            m_continuous = nullptr;
        }

        void rollback_impl()
        {
            if(m_base)
            {
                m_base->undo();
                delete m_base;
                m_base = nullptr;
            }

            if(m_continuous)
            {
                delete m_continuous;
                m_continuous = nullptr;
            }
        }

        iscore::SerializableCommand* m_base{};
        iscore::SerializableCommand* m_continuous{};
};


class CreateEventState : public CommonState
{
    public:
        CreateEventState(ObjectPath&& scenarioPath,
                         iscore::CommandStack& stack,
                         QState* parent);

    private:
        void createEventFromEventOnNothing();
        void createEventFromEventOnTimeNode();

        void createEventFromTimeNodeOnNothing();
        void createEventFromTimeNodeOnTimeNode();
        void createConstraintBetweenEvents();

        RealtimeMacroCommandDispatcher m_dispatcher;
};
