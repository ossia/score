#pragma once
#include <Engine/Executor/ProcessComponent.hpp>

namespace Device
{
class DeviceList;
}
namespace Skeleton
{
class Model;
class ProcessExecutor final :
        public ossia::time_process
{
    public:
        ProcessExecutor(const Device::DeviceList&);

        void start(ossia::state&) override;
        void stop() override;
        void pause() override;
        void resume() override;

        ossia::state_element state(ossia::time_value date, double pos) override;

    private:
        const Device::DeviceList& m_devices;
};

class ProcessExecutorComponent final :
        public Engine::Execution::ProcessComponent_T<
            Skeleton::Model,
            Skeleton::ProcessExecutor>
{
        COMPONENT_METADATA("00000000-0000-0000-0000-000000000000")
    public:
        ProcessExecutorComponent(
                Engine::Execution::IntervalComponent& parentInterval,
                Model& element,
                const Engine::Execution::Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent);
};


using ProcessExecutorComponentFactory =
    Engine::Execution::ProcessComponentFactory_T<ProcessExecutorComponent>;

}
