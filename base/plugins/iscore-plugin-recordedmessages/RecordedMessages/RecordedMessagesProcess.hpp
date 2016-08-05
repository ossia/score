#pragma once
#include <ossia/editor/scenario/time_process.hpp>
#include <QString>
#include <memory>
#include <OSSIA/Executor/ProcessElement.hpp>
#include <OSSIA/Executor/ExecutorContext.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <RecordedMessages/RecordedMessagesProcessModel.hpp>
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

namespace RecordedMessages
{
class ProcessModel;
namespace Executor
{
class ProcessExecutor final :
        public ossia::time_process
{
    public:
        ProcessExecutor(
                const Explorer::DeviceDocumentPlugin& devices,
                const RecordedMessagesList& lst);

        ossia::state_element state(double);
        ossia::state_element state() override;
        ossia::state_element offset(ossia::time_value) override;

    private:
        const Device::DeviceList& m_devices;
        RecordedMessagesList m_list;
};


class Component final :
        public ::RecreateOnPlay::ProcessComponent_T<RecordedMessages::ProcessModel, ProcessExecutor>
{
        COMPONENT_METADATA("bfcdcd2a-be3c-4bb1-bcca-240a6435b06b")
    public:
        Component(
                RecreateOnPlay::ConstraintElement& parentConstraint,
                RecordedMessages::ProcessModel& element,
                const RecreateOnPlay::Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent);
};

using ComponentFactory = ::RecreateOnPlay::ProcessComponentFactory_T<Component>;

}
}
