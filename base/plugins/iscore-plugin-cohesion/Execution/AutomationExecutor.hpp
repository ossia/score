#pragma once
#include <ProcessInterface/ProcessExecutor.hpp>
#include <State/Message.hpp>
class AutomationModel;
class DeviceInterface;
class Node;
class AutomationExecutor : public ProcessExecutor
{
    AutomationModel& m_model;
    DeviceInterface* m_dev{};
    Node* m_demodel{};
    iscore::Message m;

  public:
    AutomationExecutor(AutomationModel& model);

  public:
    void start() override;
    void stop() override;
    void onTick(const TimeValue& time) override;
};
