#pragma once
#include <ossia/editor/scenario/time_process.hpp>
#include <QJSEngine>
#include <QJSValue>
#include <QString>
#include <memory>
#include <OSSIA/Executor/ProcessElement.hpp>
#include <OSSIA/Executor/ExecutorContext.hpp>
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
namespace RecreateOnPlay
{
class ConstraintElement;
}
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

        OSSIA::StateElement state(double);
        OSSIA::StateElement state() override;
        OSSIA::StateElement offset(OSSIA::TimeValue) override;

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

using ComponentFactory = ::RecreateOnPlay::ProcessComponentFactory_T<Component>;

}
}
