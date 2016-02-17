#pragma once
#include <OSSIA/ProcessModel/TimeProcessWithConstraint.hpp>
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

namespace RecordedMessages
{
class ProcessModel;
namespace Executor
{
class ProcessExecutor final :
        public TimeProcessWithConstraint
{
    public:
        ProcessExecutor(
                const Explorer::DeviceDocumentPlugin& devices);

        std::shared_ptr<OSSIA::StateElement> state() override;

    private:
        const Device::DeviceList& m_devices;
};


class ProcessComponent final : public RecreateOnPlay::ProcessComponent
{
    public:
        ProcessComponent(
                RecreateOnPlay::ConstraintElement& parentConstraint,
                RecordedMessages::ProcessModel& element,
                const RecreateOnPlay::Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent);

    private:
        const Key &key() const override;
};


class ProcessComponentFactory final :
        public RecreateOnPlay::ProcessComponentFactory
{
    public:
        virtual ~ProcessComponentFactory();

        virtual RecreateOnPlay::ProcessComponent* make(
                RecreateOnPlay::ConstraintElement& cst,
                Process::ProcessModel& proc,
                const RecreateOnPlay::Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent) const override;

        const ConcreteFactoryKey& concreteFactoryKey() const override;

        bool matches(
                Process::ProcessModel& proc,
                const RecreateOnPlay::DocumentPlugin&,
                const iscore::DocumentContext &) const override;
};
}
}
