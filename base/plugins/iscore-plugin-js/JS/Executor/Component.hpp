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
class ProcessModel;
namespace Executor
{
class ProcessExecutor final :
        public OSSIA::TimeProcess
{
    public:
        ProcessExecutor(
                const Explorer::DeviceDocumentPlugin& devices);

        void setTickFun(const QString& val);

        std::shared_ptr<OSSIA::StateElement> state(double);
        std::shared_ptr<OSSIA::StateElement> state() override;
        std::shared_ptr<OSSIA::StateElement> offset(const OSSIA::TimeValue &) override;

    private:
        const Device::DeviceList& m_devices;
        QJSEngine m_engine;
        QJSValue m_tickFun;
};


class Component final :
        public ::RecreateOnPlay::ProcessComponent_T<JS::ProcessModel>
{
        COMPONENT_METADATA("c2737929-231e-4d57-9088-a2a3a8d3c24e")
    public:
        Component(
                RecreateOnPlay::ConstraintElement& parentConstraint,
                JS::ProcessModel& element,
                const RecreateOnPlay::Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent);
};

EXECUTOR_PROCESS_COMPONENT_FACTORY(ComponentFactory, "7beefd3e-af2c-4a20-9f5b-01283ae1c072", Component, JS::ProcessModel)

}
}
