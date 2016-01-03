#pragma once
#include <OSSIA/ProcessModel/TimeProcessWithConstraint.hpp>

class DeviceDocumentPlugin;
class DeviceList;

namespace Space
{
class ProcessModel;
namespace Executor
{
class ProcessExecutor final :
        public TimeProcessWithConstraint
{
    public:
        ProcessExecutor(
                Space::ProcessModel& process,
                DeviceDocumentPlugin& devices);


        std::shared_ptr<OSSIA::StateElement> state(
                const OSSIA::TimeValue&,
                const OSSIA::TimeValue&) override;

        const std::shared_ptr<OSSIA::State>& getStartState() const override
        {
            return m_start;
        }

        const std::shared_ptr<OSSIA::State>& getEndState() const override
        {
            return m_end;
        }


    private:
        Space::ProcessModel& m_process;
        DeviceList& m_devices;

        std::shared_ptr<OSSIA::State> m_start;
        std::shared_ptr<OSSIA::State> m_end;
};

}
}
