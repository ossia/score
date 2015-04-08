#pragma once
#include <iscore/command/OngoingCommandManager.hpp>
#include "Process/Temporal/StateMachines/StateMachineCommon.hpp"
#include "Commands/Scenario/Creations/CreationMetaCommand.hpp"

// Creates commands on a list.
class MultiCommandDispatcher
{
    public:
        MultiCommandDispatcher(iscore::CommandStack& stack):
            m_stack{stack}
        {

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


class CreateFromEventState : public CreationState
{
    public:
        CreateFromEventState(ObjectPath&& scenarioPath,
                    iscore::CommandStack& stack,
                    QState* parent);

    private:
        void createEventFromEventOnNothing();
        void createEventFromEventOnTimeNode();

        void createConstraintBetweenEvents();

        MultiCommandDispatcher m_dispatcher;
};

class CreateFromTimeNodeState : public CreationState
{
    public:
        CreateFromTimeNodeState(ObjectPath&& scenarioPath,
                    iscore::CommandStack& stack,
                    QState* parent);

    private:
        void createSingleEventOnTimeNode();

        void createEventFromEventOnNothing();
        void createEventFromEventOnTimenode();

        void createConstraintBetweenEvents();


        MultiCommandDispatcher m_dispatcher;

        ScenarioPoint m_clickedPoint;
        id_type<EventModel> m_createdFirstEvent;
};

