#pragma once
#include <QObject>
#include <Process/Process.hpp>
#include <memory>
#include <iscore/component/Component.hpp>
#include <iscore/component/ComponentFactory.hpp>

#include <OSSIA/Executor/DocumentPlugin.hpp>
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
        public iscore::Component
{
    public:
        ProcessComponent(
                ConstraintElement& cst,
                Process::ProcessModel& proc,
                const Id<iscore::Component>& id,
                const QString& name,
                QObject* parent):
            iscore::Component{id, name, parent},
            m_parent_constraint{cst},
            m_iscore_process{proc}
        {

        }

        virtual ~ProcessComponent();

        virtual void stop()
        {
            m_iscore_process.stopExecution();
        }

        auto& iscoreProcess() const
        { return m_iscore_process; }
        auto& OSSIAProcess() const
        { return m_ossia_process; }

    protected:
        ConstraintElement& m_parent_constraint;
        Process::ProcessModel& m_iscore_process;
        std::shared_ptr<OSSIA::TimeProcess> m_ossia_process;
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

using ProcessComponentFactoryList =
    iscore::GenericComponentFactoryList<
            Process::ProcessModel,
            RecreateOnPlay::DocumentPlugin,
            RecreateOnPlay::ProcessComponentFactory>;
}


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
        std::shared_ptr<OSSIA::State> m_ossia_process;
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

        virtual ProcessComponent* make(
                  StateElement& cst,
                  Process::StateProcess& proc,
                  const Context& ctx,
                  const Id<iscore::Component>& id,
                  QObject* parent) const = 0;

        virtual std::shared_ptr<OSSIA::State> make(
                  Process::StateProcess& proc,
                  const Context& ctxt) const = 0;
};

using StateProcessComponentFactoryList =
    iscore::GenericComponentFactoryList<
            Process::StateProcess,
            RecreateOnPlay::DocumentPlugin,
            RecreateOnPlay::StateProcessComponentFactory>;


}

