#pragma once
#include <iscore/command/OngoingCommandManager.hpp>
#include "Process/Temporal/StateMachines/StateMachineCommon.hpp"
#include "Commands/Scenario/Creations/CreationMetaCommand.hpp"
class ScenarioStateMachine;
// Creates commands on a list.
class MultiOngoingCommandDispatcher
{
    public:
        MultiOngoingCommandDispatcher(iscore::CommandStack& stack):
            m_stack{stack}
        {

        }

        ~MultiOngoingCommandDispatcher()
        {
            for(int i = m_cmds.size() - 1; i >= 0; --i)
            {
                delete m_cmds[i];
            }
         }

        iscore::CommandStack& stack() const
        { return m_stack; }

        void submitCommand(iscore::SerializableCommand* cmd)
        {
            if(m_cmds.empty())
            {
                cmd->redo();
                m_cmds.append(cmd);
            }
            else
            {
                iscore::SerializableCommand* last = m_cmds.last();
                if(last->mergeWith(cmd))
                {
                    last->redo();
                    delete cmd;
                }
                else
                {
                    cmd->redo();
                    m_cmds.append(cmd);
                }
            }
        }

        template<typename TheCommand, typename... Args>
        void submitCommand(Args&&... args)
        {
            if(m_cmds.empty())
            {
                auto cmd = new TheCommand(std::forward<Args>(args)...);
                cmd->redo();
                m_cmds.append(cmd);
            }
            else
            {
                iscore::SerializableCommand* last = m_cmds.last();
                if(last->uid() == TheCommand::static_uid())
                {
                    static_cast<TheCommand*>(last)->update(std::forward<Args>(args)...);
                    static_cast<TheCommand*>(last)->redo();
                }
                else
                {
                    auto cmd = new TheCommand(std::forward<Args>(args)...);
                    cmd->redo();
                    m_cmds.append(cmd);
                }
            }
        }

        template<typename CommitCommand>
        void commit()
        {
            auto theCmd = new CommitCommand;
            for(auto& cmd : m_cmds)
            {
                theCmd->addCommand(cmd);
            }

            SendStrategy::Quiet::send(stack(), theCmd);
            m_cmds.clear();
        }

        void rollback()
        {
            for(int i = m_cmds.size() - 1; i >= 0; --i)
            {
                m_cmds[i]->undo();
                delete m_cmds[i];
            }

            m_cmds.clear();
        }


    private:
        iscore::CommandStack& m_stack;
        QList<iscore::SerializableCommand*> m_cmds;
};


/* OLD
class MultiCommandDispatcher
{
    public:
        MultiCommandDispatcher(iscore::CommandStack& stack):
            m_stack{stack}
        {

        }

        ~MultiCommandDispatcher()
        {
            for(int i = m_cmds.size() - 1; i >= 0; --i)
            {
                delete m_cmds[i];
            }
         }

        iscore::CommandStack& stack() const
        { return m_stack; }

        void submitCommand(iscore::SerializableCommand* cmd)
        {
            if(m_cmds.empty())
            {
                cmd->redo();
                m_cmds.append(cmd);
            }
            else
            {
                iscore::SerializableCommand* last = m_cmds.last();
                if(last->mergeWith(cmd))
                {
                    last->redo();
                    delete cmd;
                }
                else
                {
                    cmd->redo();
                    m_cmds.append(cmd);
                }
            }
        }

        void commit()
        {
            auto theCmd = new Scenario::Command::CreationMetaCommand;
            for(auto& cmd : m_cmds)
            {
                theCmd->addCommand(cmd);
            }

            SendStrategy::Quiet::send(stack(), theCmd);
            m_cmds.clear();
        }

        void rollback()
        {
            for(int i = m_cmds.size() - 1; i >= 0; --i)
            {
                m_cmds[i]->undo();
                delete m_cmds[i];
            }

            m_cmds.clear();
        }


    private:
        iscore::CommandStack& m_stack;
        QList<iscore::SerializableCommand*> m_cmds;
};
*/

/* TODO keep this in mind for later.
template<typename CmdBase, typename CmdUpdate>
class MultiCommandDispatcher2
{
    public:
        MultiCommandDispatcher2(iscore::CommandStack& stack):
            m_stack{stack}
        {

        }

        ~MultiCommandDispatcher()
        {
            delete m_base;
            delete m_update;
         }

        iscore::CommandStack& stack() const
        { return m_stack; }

        void submitCommand(CmdBase* cmd)
        {
            Q_ASSERT(m_base == nullptr);
            Q_ASSERT(m_update == nullptr);

            cmd->redo();
            m_base = cmd;
        }

        void submitCommand(CmdUpdate* cmd)
        {
            Q_ASSERT(m_base);
            Q_ASSERT(m_update == nullptr);

            cmd->redo();
            m_update = cmd;
        }

        template<typename... Args>
        void submitCommand(Args&&... args)
        {
            Q_ASSERT(m_base);
            Q_ASSERT(m_update);

            m_update->update(std::forward<Args>(args)...);
            m_update->redo();
        }

        void commit()
        {
            auto theCmd = new Scenario::Command::CreationMetaCommand;
                theCmd->addCommand(m_base);
                theCmd->addCommand(m_update);

            SendStrategy::Quiet::send(stack(), theCmd);
        }

        void rollback()
        {
            m_base->undo();

            delete m_base;
            delete m_update;
            m_base = nullptr;
            m_update = nullptr;
        }


    private:
        iscore::CommandStack& m_stack;
        CmdBase* m_base{};
        CmdUpdate* m_update{};
};
*/


class CreateFromEventState : public CreationState
{
    public:
        CreateFromEventState(
                const ScenarioStateMachine& stateMachine,
                ObjectPath&& scenarioPath,
                iscore::CommandStack& stack,
                QState* parent);

    private:
        void createEventFromEventOnNothing();
        void createEventFromEventOnTimeNode();

        void createConstraintBetweenEvents();

        MultiOngoingCommandDispatcher m_dispatcher;
};

class CreateFromTimeNodeState : public CreationState
{
    public:
        CreateFromTimeNodeState(
                const ScenarioStateMachine& stateMachine,
                ObjectPath&& scenarioPath,
                iscore::CommandStack& stack,
                QState* parent);

    private:
        void createSingleEventOnTimeNode();

        void createEventFromEventOnNothing();
        void createEventFromEventOnTimenode();

        void createConstraintBetweenEvents();


        MultiOngoingCommandDispatcher m_dispatcher;

        ScenarioPoint m_clickedPoint;
        id_type<EventModel> m_createdFirstEvent;
};

