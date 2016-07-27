#pragma once
#include <ossia/editor/value/value.hpp>
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
        OSSIA::Type m_addressType{OSSIA::Type(-1)};

        std::shared_ptr<OSSIA::CurveAbstract> on_curveChanged(const optional<OSSIA::Destination>&);

        template<typename T>
        std::shared_ptr<OSSIA::CurveAbstract> on_curveChanged_impl(const optional<OSSIA::Destination>&);

        std::shared_ptr<OSSIA::CurveAbstract> m_ossia_curve;

        const Device::DeviceList& m_deviceList;
};
using ComponentFactory = ::RecreateOnPlay::ProcessComponentFactory_T<Component>;
}
}

ISCORE_COMPONENT_FACTORY(RecreateOnPlay::ProcessComponentFactory, Automation::RecreateOnPlay::ComponentFactory)
