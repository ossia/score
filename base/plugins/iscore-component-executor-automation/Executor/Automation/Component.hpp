#pragma once
#include <Editor/Value.h>
#include <OSSIA/Executor/ProcessElement.hpp>
#include <memory>

namespace Process { class ProcessModel; }
class QObject;
namespace OSSIA {
class TimeProcess;
}  // namespace OSSIA
namespace RecreateOnPlay {
class ConstraintElement;
}  // namespace RecreateOnPlay

namespace OSSIA
{
    class Automation;
    class CurveAbstract;
}

namespace Automation
{
class ProcessModel;
}

namespace Device
{
class DeviceList;
}


namespace RecreateOnPlay
{
namespace Automation
{
class Component final : public ProcessComponent
{
    public:
        Component(
                ConstraintElement& parentConstraint,
                ::Automation::ProcessModel& element,
                const Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent);

    private:
        const Key &key() const override;
        void recreate();
        OSSIA::Value::Type m_addressType{OSSIA::Value::Type(-1)};

        std::shared_ptr<OSSIA::CurveAbstract> on_curveChanged();

        template<typename T>
        std::shared_ptr<OSSIA::CurveAbstract> on_curveChanged_impl();

        std::shared_ptr<OSSIA::CurveAbstract> m_ossia_curve;

        const Device::DeviceList& m_deviceList;
};


class ComponentFactory final :
        public ProcessComponentFactory
{
        ISCORE_COMPONENT_FACTORY(RecreateOnPlay::ProcessComponentFactory, RecreateOnPlay::Automation::ComponentFactory)
    public:
        virtual ~ComponentFactory();
        virtual ProcessComponent* make(
                ConstraintElement& cst,
                Process::ProcessModel& proc,
                const Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent) const override;

        const ConcreteFactoryKey& concreteFactoryKey() const override;

        bool matches(
                Process::ProcessModel&,
                const DocumentPlugin&,
                const iscore::DocumentContext &) const override;
};
}
}
