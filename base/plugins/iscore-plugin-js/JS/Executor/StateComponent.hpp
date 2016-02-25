#pragma once
#include <Editor/TimeProcess.h>
#include <QJSEngine>
#include <QJSValue>
#include <QString>
#include <memory>
#include <OSSIA/Executor/ProcessElement.hpp>
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
        public RecreateOnPlay::StateProcessComponent
{
    public:
        StateProcessComponent(
                RecreateOnPlay::StateElement& parentState,
                JS::StateProcess& element,
                const RecreateOnPlay::Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent);

    private:
        const Key &key() const override;
};

class StateProcessComponentFactory final :
        public RecreateOnPlay::StateProcessComponentFactory
{
    public:
        virtual ~StateProcessComponentFactory();

        virtual RecreateOnPlay::StateProcessComponent* make(
                    RecreateOnPlay::StateElement& cst,
                    Process::StateProcess& proc,
                    const RecreateOnPlay::Context& ctx,
                    const Id<iscore::Component>& id,
                    QObject* parent) const override;

        const ConcreteFactoryKey& concreteFactoryKey() const override;

        bool matches(
                Process::StateProcess& proc,
                const RecreateOnPlay::DocumentPlugin&,
                const iscore::DocumentContext &) const override;

        virtual std::shared_ptr<OSSIA::StateElement> make(
                  Process::StateProcess& proc,
                  const RecreateOnPlay::Context& ctxt) const override;
};



}
}
