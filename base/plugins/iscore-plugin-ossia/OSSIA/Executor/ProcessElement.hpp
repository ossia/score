#pragma once
#include <QObject>
#include <Process/Process.hpp>
#include <memory>
#include <iscore/component/Component.hpp>
#include <iscore/component/ComponentFactory.hpp>

#include <OSSIA/Executor/DocumentPlugin.hpp>
#include <ossia/editor/scenario/time_process.hpp>
#include <Scenario/Document/Components/ProcessComponent.hpp>
#include <iscore/plugins/customfactory/ModelFactory.hpp>
#include <iscore_plugin_ossia_export.h>
namespace ossia
{
    class scenario;
    class time_process;
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

        std::unique_ptr<ossia::time_process> give_OSSIAProcess()
        { return std::unique_ptr<ossia::time_process>(m_ossia_process); }
        auto& OSSIAProcess() const
        { return *m_ossia_process; }

    protected:
        ConstraintElement& m_parent_constraint;
        ossia::time_process* m_ossia_process{};
};

template<typename Process_T, typename OSSIA_Process_T>
struct ProcessComponent_T :
        public Scenario::GenericProcessComponent_T<ProcessComponent, Process_T>
{
        using Scenario::GenericProcessComponent_T<ProcessComponent, Process_T>::GenericProcessComponent_T;

        auto& OSSIAProcess() const
        { return static_cast<OSSIA_Process_T&>(ProcessComponent::OSSIAProcess()); }
};

class ISCORE_PLUGIN_OSSIA_EXPORT ProcessComponentFactory :
        public iscore::GenericComponentFactory<
            Process::ProcessModel,
            RecreateOnPlay::DocumentPlugin,
            RecreateOnPlay::ProcessComponentFactory>
{
        ISCORE_ABSTRACT_FACTORY("d0f714de-c832-42d8-a605-60f5ffd0b7af")
    public:
        virtual ~ProcessComponentFactory();
        virtual ProcessComponent* make(
                  ConstraintElement& cst,
                  Process::ProcessModel& proc,
                  const Context& ctx,
                  const Id<iscore::Component>& id,
                  QObject* parent) const = 0;
};

template<typename ProcessComponent_T>
class ProcessComponentFactory_T :
        public iscore::GenericComponentFactoryImpl<ProcessComponent_T, ProcessComponentFactory>
{
    public:
        using model_type = typename ProcessComponent_T::model_type;
        ProcessComponent* make(
                ConstraintElement& cst,
                Process::ProcessModel& proc,
                const Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent) const final override
        {
            return new ProcessComponent_T{
                cst, static_cast<model_type&>(proc), ctx, id, parent};
        }
};
using ProcessComponentFactoryList =
    iscore::GenericComponentFactoryList<
            Process::ProcessModel,
            RecreateOnPlay::DocumentPlugin,
            RecreateOnPlay::ProcessComponentFactory>;
}
