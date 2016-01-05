#pragma once
#include "ProcessElement.hpp"
#include <State/Address.hpp>
#include <QPointer>
#include <API/Headers/Editor/Value.h>
namespace OSSIA
{
    class Mapper;
    class CurveAbstract;
}

class MappingModel;
class DeviceList;
class ConstraintElement;


namespace RecreateOnPlay
{
class MappingElement final : public ProcessComponent
{
    public:
        MappingElement(
                ConstraintElement& parentConstraint,
                MappingModel& element,
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

        const DeviceList& m_deviceList;

        // Component interface
    public:
        const Key&key() const override;
};


class MappingComponentFactory final :
        public ProcessComponentFactory
{
    public:
        virtual ~MappingComponentFactory();
        virtual ProcessComponent* make(
                ConstraintElement& cst,
                Process::ProcessModel& proc,
                const Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent) const override;

        const factory_key_type& key_impl() const override;

        bool matches(
                Process::ProcessModel&,
                const DocumentPlugin&,
                const iscore::DocumentContext &) const override;
};
}
