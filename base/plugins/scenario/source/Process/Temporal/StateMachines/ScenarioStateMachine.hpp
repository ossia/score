#pragma once

#include "StateMachineVeryCommon.hpp"

#include <iscore/command/OngoingCommandManager.hpp>

#include <QStateMachine>

class TemporalScenarioPresenter;
class ScenarioModel;
class ScenarioStateMachine : public QStateMachine
{
    public:
        ScenarioPoint currentMousePoint;

        ScenarioStateMachine(TemporalScenarioPresenter& presenter);

        TemporalScenarioPresenter& presenter()
        { return m_presenter; }
        const ScenarioModel& model() const;

        iscore::CommandStack& commandStack()
        { return m_commandStack; }

    private:
        TemporalScenarioPresenter& m_presenter;
        iscore::CommandStack& m_commandStack;
};
