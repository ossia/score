#pragma once
#include <ProcessInterface/ProcessExecutor.hpp>
class ScenarioModel;

class EventExecutor;
class ConstraintExecutor;
class ScenarioExecutor : public ProcessExecutor
{
    ScenarioModel& m_model;

    QList<EventExecutor*> m_events;
    QList<ConstraintExecutor*> m_constraints;
  public:
    ScenarioExecutor(ScenarioModel& model);

  public:
    void start() override;
    void stop() override;
    void onTick(const TimeValue& time) override;
};
