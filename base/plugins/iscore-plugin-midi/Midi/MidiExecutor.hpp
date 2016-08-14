#pragma once
#include <ossia/editor/scenario/time_process.hpp>
#include <Engine/Executor/ProcessElement.hpp>
#include <Engine/Executor/ExecutorContext.hpp>

namespace ossia
{
namespace net
{
class node_base;
}
}
namespace Device
{
class DeviceList;
}
namespace Midi
{
class ProcessModel;
namespace Executor
{
class ProcessExecutor final :
        public ossia::time_process
{
    public:
        ProcessExecutor(
                const Midi::ProcessModel& proc,
                const Device::DeviceList& devices);
        ~ProcessExecutor();

        ossia::state_element state(double);
        ossia::state_element state() override;
        ossia::state_element offset(ossia::time_value) override;

    private:
        const Midi::ProcessModel& m_process;
        const Device::DeviceList& m_devices;

        ossia::net::node_base* m_channelNode{};
};

}
}
