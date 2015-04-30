#pragma once
#include <iscore/command/OngoingCommandManager.hpp>
#include "Process/Temporal/StateMachines/StateMachineCommon.hpp"
class ScenarioStateMachine;



class SingleOngoingCommandDispatcher
{
    public:
        SingleOngoingCommandDispatcher(iscore::CommandStack& stack):
            m_stack{stack}
        {

        }

        ~SingleOngoingCommandDispatcher()
        {
            delete m_cmd;
         }

        iscore::CommandStack& stack() const
        { return m_stack; }

        template<typename TheCommand, typename... Args>
        void submitCommand(Args&&... args)
        {
            if(!m_cmd)
            {
                auto cmd = new TheCommand(std::forward<Args>(args)...);
                cmd->redo();
                m_cmd = cmd;
            }
            else
            {
                Q_ASSERT(m_cmd->uid() == TheCommand::static_uid());
                static_cast<TheCommand*>(m_cmd)->update(std::forward<Args>(args)...);
                static_cast<TheCommand*>(m_cmd)->redo();
            }
        }

        void commit()
        {
            SendStrategy::Quiet::send(stack(), m_cmd);
            m_cmd = nullptr;
        }

        void rollback()
        {
            m_cmd->undo();
            delete m_cmd;
            m_cmd = nullptr;
        }


    private:
        iscore::CommandStack& m_stack;
        iscore::SerializableCommand* m_cmd{};
};

class MoveConstraintState : public CommonScenarioState
{
    public:
        MoveConstraintState(const ScenarioStateMachine& stateMachine,
                            ObjectPath&& scenarioPath,
                            iscore::CommandStack& stack,
                            iscore::ObjectLocker& locker,
                            QState* parent);

    SingleOngoingCommandDispatcher m_dispatcher;

    private:
        TimeValue m_constraintInitialClickDate;
        TimeValue m_constraintInitialStartDate;
};

class MoveEventState : public CommonScenarioState
{
    public:
        MoveEventState(const ScenarioStateMachine& stateMachine,
                       ObjectPath&& scenarioPath,
                       iscore::CommandStack& stack,
                       iscore::ObjectLocker& locker,
                       QState* parent);

        SingleOngoingCommandDispatcher m_dispatcher;
};

class MoveTimeNodeState : public CommonScenarioState
{
    public:
        MoveTimeNodeState(const ScenarioStateMachine& stateMachine,
                          ObjectPath&& scenarioPath,
                          iscore::CommandStack& stack,
                          iscore::ObjectLocker& locker,
                          QState* parent);

        SingleOngoingCommandDispatcher m_dispatcher;
};
