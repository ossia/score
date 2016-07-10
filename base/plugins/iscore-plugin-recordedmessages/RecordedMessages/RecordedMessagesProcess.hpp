#pragma once
#include <Editor/TimeProcess.h>
#include <QString>
#include <memory>
#include <OSSIA/Executor/ProcessElement.hpp>
#include <OSSIA/Executor/ExecutorContext.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <RecordedMessages/RecordedMessagesProcessModel.hpp>
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

namespace RecordedMessages
{
class ProcessModel;
namespace Executor
{
class ProcessExecutor final :
        public OSSIA::TimeProcess
{
    public:
        ProcessExecutor(
                const Explorer::DeviceDocumentPlugin& devices,
                const RecordedMessagesList& lst);

        std::shared_ptr<OSSIA::StateElement> state(double);
        std::shared_ptr<OSSIA::StateElement> state() override;
        std::shared_ptr<OSSIA::StateElement> offset(const OSSIA::TimeValue &) override;

    private:
        const Device::DeviceList& m_devices;
        RecordedMessagesList m_list;
};


class Component final :
        public ::RecreateOnPlay::ProcessComponent_T<RecordedMessages::ProcessModel>
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
