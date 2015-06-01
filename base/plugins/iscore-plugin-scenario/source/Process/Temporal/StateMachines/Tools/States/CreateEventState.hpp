#pragma once
#include <iscore/command/OngoingCommandManager.hpp>
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseStates.hpp"
#include "Commands/Scenario/Creations/CreationMetaCommand.hpp"
class ScenarioStateMachine;
// Creates commands on a list.
// TODO put this in ongoincommanddispatcher
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
            cmd->redo();
            m_cmds.append(cmd);
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

