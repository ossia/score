#pragma once
#include <QObject>
#include <Process/Process.hpp>
#include <memory>
#include <iscore/component/Component.hpp>
#include <iscore/component/ComponentFactory.hpp>

#include <OSSIA/Executor/DocumentPlugin.hpp>
#include <Scenario/Document/Components/ProcessComponent.hpp>
#include <iscore_plugin_ossia_export.h>
namespace OSSIA
{
    class Scenario;
    class TimeProcess;
}

// TODO RENAMEME
namespace RecreateOnPlay
{
struct Context;
class ConstraintElement;

class ISCORE_PLUGIN_OSSIA_EXPORT ProcessComponent :
        public Scenario::GenericProcessComponent<const Context>
{
    public:
        ProcessComponent(
                ConstraintElement& cst,
                Process::ProcessModel& proc,
                const Context& ctx,
                const Id<iscore::Component>& id,
                const QString& name,
                QObject* parent):
            Scenario::GenericProcessComponent<const Context>{proc, ctx, id, name, parent},
            m_parent_constraint{cst}
        {

        }

        virtual ~ProcessComponent();

        virtual void stop()
        {
            process().stopExecution();
        }

        auto& OSSIAProcess() const
        { return m_ossia_process; }

    protected:
        ConstraintElement& m_parent_constraint;
        std::shared_ptr<OSSIA::TimeProcess> m_ossia_process;
};

template<typename Process_T>
class ProcessComponent_T : public ProcessComponent
{
    public:
        using ProcessComponent::ProcessComponent;

        const Process_T& process() const
        { return static_cast<const Process_T&>(ProcessComponent::process()); }
};

class ISCORE_PLUGIN_OSSIA_EXPORT ProcessComponentFactory :
        public iscore::GenericComponentFactory<
            Process::ProcessModel,
            RecreateOnPlay::DocumentPlugin,
            RecreateOnPlay::ProcessComponentFactory>
{
        ISCORE_ABSTRACT_FACTORY_DECL(
                RecreateOnPlay::ProcessComponentFactory,
                "d0f714de-c832-42d8-a605-60f5ffd0b7af")
    public:
        virtual ~ProcessComponentFactory();
        virtual ProcessComponent* make(
                  ConstraintElement& cst,
                  Process::ProcessModel& proc,
                  const Context& ctx,
                  const Id<iscore::Component>& id,
                  QObject* parent) const = 0;
};

template<
        typename ProcessComponent_T,
        typename Process_T>
class ProcessComponentFactory_T : public ProcessComponentFactory
{
    public:
        using ProcessComponentFactory::ProcessComponentFactory;

        bool matches(
                Process::ProcessModel& p, const DocumentPlugin&) const final override
        {
            return dynamic_cast<Process_T*>(&p);
        }

        ProcessComponent* make(
                ConstraintElement& cst,
                Process::ProcessModel& proc,
                const Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent) const final override
        {
            return new ProcessComponent_T{
                cst, static_cast<Process_T&>(proc), ctx, id, parent};
        }
};
using ProcessComponentFactoryList =
    iscore::GenericComponentFactoryList<
            Process::ProcessModel,
            RecreateOnPlay::DocumentPlugin,
            RecreateOnPlay::ProcessComponentFactory>;
}


#define EXECUTOR_PROCESS_COMPONENT_FACTORY(FactoryName, Uuid, ProcessComponent, Process) \
class FactoryName final : \
        public ::RecreateOnPlay::ProcessComponentFactory_T<ProcessComponent, Process> \
{ \
        ISCORE_CONCRETE_FACTORY_DECL(Uuid)  \
};


///// State processes
#include <Process/StateProcess.hpp>
#include <OSSIA/Executor/StateElement.hpp>
#include <Editor/State.h>
namespace RecreateOnPlay
{
class ISCORE_PLUGIN_OSSIA_EXPORT StateProcessComponent :
        public iscore::Component
{
    public:
        StateProcessComponent(
                StateElement& state,
                Process::StateProcess& proc,
                const Id<iscore::Component>& id,
                const QString& name,
                QObject* parent):
            iscore::Component{id, name, parent},
            m_parent_state{state},
            m_iscore_process{proc}
        {

        }

        virtual ~StateProcessComponent();

        auto& iscoreProcess() const
        { return m_iscore_process; }

        auto& OSSIAProcess() const
        { return m_ossia_process; }

    protected:
        StateElement& m_parent_state;
        Process::StateProcess& m_iscore_process;
        std::shared_ptr<OSSIA::StateElement> m_ossia_process;
};

class ISCORE_PLUGIN_OSSIA_EXPORT StateProcessComponentFactory :
        public iscore::GenericComponentFactory<
            Process::StateProcess,
            RecreateOnPlay::DocumentPlugin,
            RecreateOnPlay::ProcessComponentFactory>
{
        ISCORE_ABSTRACT_FACTORY_DECL(
                RecreateOnPlay::ProcessComponentFactory,
                "cef1b394-84b2-4241-b4eb-72b1fb504f92")
    public:
        virtual ~StateProcessComponentFactory();

        virtual StateProcessComponent* make(
                  StateElement& cst,
                  Process::StateProcess& proc,
                  const Context& ctx,
                  const Id<iscore::Component>& id,
                  QObject* parent) const = 0;

        virtual std::shared_ptr<OSSIA::StateElement> make(
                  Process::StateProcess& proc,
                  const Context& ctxt) const = 0;
};

using StateProcessComponentFactoryList =
    iscore::GenericComponentFactoryList<
            Process::StateProcess,
            RecreateOnPlay::DocumentPlugin,
            RecreateOnPlay::StateProcessComponentFactory>;


}
