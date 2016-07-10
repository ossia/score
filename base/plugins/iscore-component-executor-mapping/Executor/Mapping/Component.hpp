#pragma once
#include <OSSIA/Executor/ProcessElement.hpp>

#include <State/Address.hpp>
#include <Mapping/MappingModel.hpp>
#include <QPointer>
#include <Editor/Value.h>
namespace OSSIA
{
    class Mapper;
    class CurveAbstract;
}
namespace Device
{
class DeviceList;
}

namespace Mapping
{
namespace RecreateOnPlay
{
class Component final :
        public ::RecreateOnPlay::ProcessComponent_T<Mapping::ProcessModel>
{
        COMPONENT_METADATA("da360b58-9885-4106-be54-8e272ed45dbe")
    public:
        Component(
                ::RecreateOnPlay::ConstraintElement& parentConstraint,
                ::Mapping::ProcessModel& element,
                const ::RecreateOnPlay::Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent);

    private:
        void recreate();
        std::shared_ptr<OSSIA::CurveAbstract> rebuildCurve();

        OSSIA::Value::Type m_sourceAddressType{OSSIA::Value::Type(-1)};
        OSSIA::Value::Type m_targetAddressType{OSSIA::Value::Type(-1)};

        template<typename T>
        std::shared_ptr<OSSIA::CurveAbstract> on_curveChanged_impl();

        template<typename X_T, typename Y_T>
        std::shared_ptr<OSSIA::CurveAbstract> on_curveChanged_impl2();

        std::shared_ptr<OSSIA::CurveAbstract> m_ossia_curve;

        const Device::DeviceList& m_deviceList;
};

EXECUTOR_PROCESS_COMPONENT_FACTORY(ComponentFactory, "53bde917-0c67-4c5c-b490-fbe79f92633e", Component, Mapping::ProcessModel)

}
}
