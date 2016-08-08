#pragma once
#include <ossia/editor/value/value.hpp>
#include <OSSIA/Executor/ProcessElement.hpp>
#include <memory>
#include <Automation/AutomationModel.hpp>
#include <ossia/editor/automation/automation.hpp>
namespace ossia
{
    class curve_abstract;
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
        public ::Engine::Execution::ProcessComponent_T<Automation::ProcessModel, ossia::automation>
{
        COMPONENT_METADATA("f759eacd-5a67-4627-bbe8-c649e0f9b6c5")
    public:
        Component(
                ::Engine::Execution::ConstraintElement& parentConstraint,
                Automation::ProcessModel& element,
                const ::Engine::Execution::Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent);

    private:
        void recreate();
        ossia::val_type m_addressType{ossia::val_type(-1)};

        std::shared_ptr<ossia::curve_abstract> on_curveChanged(const optional<ossia::Destination>&);

        template<typename T>
        std::shared_ptr<ossia::curve_abstract> on_curveChanged_impl(const optional<ossia::Destination>&);

        std::shared_ptr<ossia::curve_abstract> m_ossia_curve;

        const Device::DeviceList& m_deviceList;
};
using ComponentFactory = ::Engine::Execution::ProcessComponentFactory_T<Component>;
}
}

ISCORE_COMPONENT_FACTORY(Engine::Execution::ProcessComponentFactory, Automation::RecreateOnPlay::ComponentFactory)
