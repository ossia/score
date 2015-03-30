#pragma once
#include <ProcessInterface/ProcessExecutor.hpp>
class ScenarioModel;

class EventExecutor;
class TimeNodeExecutor;
class ConstraintExecutor;
class ScenarioExecutor : public ProcessExecutor
{
    ScenarioModel& m_model;

    QSet<EventExecutor*> m_events;
    QSet<ConstraintExecutor*> m_constraints;
    QSet<TimeNodeExecutor*> m_timenodes;
  public:
    ScenarioExecutor(ScenarioModel& model);

  public:
    void start() override;
    void stop() override;
    void onTick(const TimeValue& time) override;
};
