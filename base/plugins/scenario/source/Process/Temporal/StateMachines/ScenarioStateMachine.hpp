#pragma once

#include "StateMachineVeryCommon.hpp"

#include <iscore/command/OngoingCommandManager.hpp>

#include <QStateMachine>

class TemporalScenarioPresenter;
class ScenarioModel;
class ScenarioStateMachine : public QStateMachine
{
    public:
        QPointF scenePoint;
        ScenarioPoint scenarioPoint;

        ScenarioStateMachine(TemporalScenarioPresenter& presenter);

        const TemporalScenarioPresenter& presenter() const
        { return m_presenter; }
        const ScenarioModel& model() const;

        iscore::CommandStack& commandStack() const
        { return m_commandStack; }
        iscore::ObjectLocker& locker() const
        { return m_locker; }

    private:
        TemporalScenarioPresenter& m_presenter;
        iscore::CommandStack& m_commandStack;
        iscore::ObjectLocker& m_locker;
};
