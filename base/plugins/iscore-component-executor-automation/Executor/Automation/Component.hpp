#pragma once
#include <Editor/Value.h>
#include <OSSIA/Executor/ProcessElement.hpp>
#include <memory>
#include <Automation/AutomationModel.hpp>

namespace OSSIA
{
    class Automation;
    class CurveAbstract;
}

namespace Device
{
class DeviceList;
}


namespace Automation
{
namespace RecreateOnPlay
{
class Component final :
        public ::RecreateOnPlay::ProcessComponent_T<Automation::ProcessModel>
{
        COMPONENT_METADATA("f759eacd-5a67-4627-bbe8-c649e0f9b6c5")
    public:
        Component(
                ::RecreateOnPlay::ConstraintElement& parentConstraint,
                Automation::ProcessModel& element,
                const ::RecreateOnPlay::Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent);

    private:
        void recreate();
        OSSIA::Value::Type m_addressType{OSSIA::Value::Type(-1)};

        std::shared_ptr<OSSIA::CurveAbstract> on_curveChanged();

        template<typename T>
        std::shared_ptr<OSSIA::CurveAbstract> on_curveChanged_impl();

        std::shared_ptr<OSSIA::CurveAbstract> m_ossia_curve;

        const Device::DeviceList& m_deviceList;
};

EXECUTOR_PROCESS_COMPONENT_FACTORY(ComponentFactory, "a20bb032-2dfc-4048-acd2-7060c6af9c76", Component, Automation::ProcessModel)
}
}

ISCORE_COMPONENT_FACTORY(RecreateOnPlay::ProcessComponentFactory, Automation::RecreateOnPlay::ComponentFactory)
