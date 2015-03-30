#pragma once
#include <ProcessInterface/ProcessExecutor.hpp>
class AutomationModel;

class AutomationExecutor : public ProcessExecutor
{
    AutomationModel& m_model;

  public:
    AutomationExecutor(AutomationModel& model);

  public:
    void start() override;
    void stop() override;
    void onTick(const TimeValue& time) override;
};
