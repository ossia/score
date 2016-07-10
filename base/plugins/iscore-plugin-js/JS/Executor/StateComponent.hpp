#pragma once
#include <Editor/TimeProcess.h>
#include <QJSEngine>
#include <QJSValue>
#include <QString>
#include <memory>
#include <OSSIA/Executor/StateProcessComponent.hpp>
#include <OSSIA/Executor/ExecutorContext.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include "Editor/TimeValue.h"

namespace Explorer
{
class DeviceDocumentPlugin;
}
namespace Device
{
class DeviceList;
}
namespace RecreateOnPlay
{
class ConstraintElement;
}
namespace OSSIA {
class State;
class StateElement;
}  // namespace OSSIA

namespace JS
{
class StateProcess;
namespace Executor
{
class State :
        public OSSIA::StateElement
{
    public:
        State(
            const QString& script,
            const Explorer::DeviceDocumentPlugin& devices);

        void launch() const override;

        OSSIA::StateElement::Type getType() const override
        {
            return OSSIA::StateElement::Type::USER;
        }

        const Device::DeviceList& m_devices;
        mutable QJSEngine m_engine;
        mutable QJSValue m_fun;
};

class StateProcessComponent final :
        public RecreateOnPlay::StateProcessComponent_T<JS::StateProcess>
{
        COMPONENT_METADATA("068c116f-9d1f-47d0-bd43-335792ba1a6a")
    public:
        StateProcessComponent(
                RecreateOnPlay::StateElement& parentState,
                JS::StateProcess& element,
                const RecreateOnPlay::Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent);

        static std::shared_ptr<OSSIA::StateElement> make(
                Process::StateProcess& proc,
                const RecreateOnPlay::Context& ctxt);
};

using StateProcessComponentFactory = RecreateOnPlay::StateProcessComponentFactory_T<StateProcessComponent>;
}
}
