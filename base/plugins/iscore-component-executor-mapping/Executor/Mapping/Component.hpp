#pragma once
#include <OSSIA/Executor/ProcessElement.hpp>

#include <State/Address.hpp>
#include <QPointer>
#include <Editor/Value.h>
namespace OSSIA
{
    class Mapper;
    class CurveAbstract;
}

namespace Mapping
{
class MappingModel;
}

namespace Device
{
class DeviceList;
}
class ConstraintElement;


namespace RecreateOnPlay
{
namespace Mapping
{
class Component final : public ProcessComponent
{
    public:
        Component(
                ConstraintElement& parentConstraint,
                ::Mapping::MappingModel& element,
                const Context& ctx,
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

        // Component interface
    public:
        const Key&key() const override;
};


class ComponentFactory final :
        public ProcessComponentFactory
{
        ISCORE_COMPONENT_FACTORY(RecreateOnPlay::ProcessComponentFactory, RecreateOnPlay::Mapping::ComponentFactory)
    public:
        virtual ~ComponentFactory();
        virtual ProcessComponent* make(
                ConstraintElement& cst,
                Process::ProcessModel& proc,
                const Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent) const override;

        const factory_key_type& concreteFactoryKey() const override;

        bool matches(
                Process::ProcessModel&,
                const DocumentPlugin&,
                const iscore::DocumentContext &) const override;
};
}
}
