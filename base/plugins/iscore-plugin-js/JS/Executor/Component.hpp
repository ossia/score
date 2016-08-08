#pragma once
#include <ossia/editor/scenario/time_process.hpp>
#include <QJSEngine>
#include <QJSValue>
#include <QString>
#include <memory>
#include <Engine/Executor/ProcessElement.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <ossia/editor/scenario/time_value.hpp>

namespace Explorer
{
class DeviceDocumentPlugin;
}
namespace Device
{
class DeviceList;
}
namespace Engine { namespace Execution
{
class ConstraintElement;
} }
namespace JS
{
class ProcessModel;
namespace Executor
{
class ProcessExecutor final :
        public ossia::time_process
{
    public:
        ProcessExecutor(
                const Explorer::DeviceDocumentPlugin& devices);

        void setTickFun(const QString& val);

        ossia::state_element state(double);
        ossia::state_element state() override;
        ossia::state_element offset(ossia::time_value) override;

    private:
        const Device::DeviceList& m_devices;
        QJSEngine m_engine;
        QJSValue m_tickFun;
};


class Component final :
        public ::Engine::Execution::ProcessComponent_T<JS::ProcessModel, ProcessExecutor>
{
        COMPONENT_METADATA("c2737929-231e-4d57-9088-a2a3a8d3c24e")
    public:
        Component(
                Engine::Execution::ConstraintElement& parentConstraint,
                JS::ProcessModel& element,
                const Engine::Execution::Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent);
};

using ComponentFactory = ::Engine::Execution::ProcessComponentFactory_T<Component>;

}
}
