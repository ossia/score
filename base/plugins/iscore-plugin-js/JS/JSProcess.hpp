#pragma once
#include <OSSIA/ProcessModel/TimeProcessWithConstraint.hpp>
#include <QJSEngine>
#include <QJSValue>
#include <QString>
#include <memory>
#include <OSSIA/Executor/ProcessElement.hpp>
#include <OSSIA/Executor/ExecutorContext.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include "Editor/TimeValue.h"

namespace DeviceExplorer
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
        public TimeProcessWithConstraint
{
    public:
        ProcessExecutor(
                const DeviceExplorer::DeviceDocumentPlugin& devices);

        void setTickFun(const QString& val);

        std::shared_ptr<OSSIA::StateElement> state(
                const OSSIA::TimeValue&,
                const OSSIA::TimeValue&) override;

        const std::shared_ptr<OSSIA::State>& getStartState() const override
        {
            return m_start;
        }

        const std::shared_ptr<OSSIA::State>& getEndState() const override
        {
            return m_end;
        }


    private:
        const Device::DeviceList& m_devices;
        QJSEngine m_engine;
        QJSValue m_tickFun;

        std::shared_ptr<OSSIA::State> m_start;
        std::shared_ptr<OSSIA::State> m_end;
};


class ProcessComponent final : public RecreateOnPlay::ProcessComponent
{
    public:
        ProcessComponent(
                RecreateOnPlay::ConstraintElement& parentConstraint,
                JS::ProcessModel& element,
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
