#pragma once
#include <Process/StateProcess.hpp>

#include <iscore/component/Component.hpp>
#include <iscore/component/ComponentFactory.hpp>

#include <iscore_plugin_ossia_export.h>

#include <Scenario/Document/Components/ProcessComponent.hpp>
#include <OSSIA/Executor/StateElement.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>
#include <Editor/State.h>
#include <iscore/plugins/customfactory/ModelFactory.hpp>
namespace RecreateOnPlay
{
class ISCORE_PLUGIN_OSSIA_EXPORT StateProcessComponent :
        public Scenario::GenericStateProcessComponent<const Context>
{
    public:
        StateProcessComponent(
                StateElement& state,
                Process::StateProcess& proc,
                const Context& ctx,
                const Id<iscore::Component>& id,
                const QString& name,
                QObject* parent):
            Scenario::GenericStateProcessComponent<const Context>{proc, ctx, id, name, parent},
            m_parent_state{state}
        {

        }

        virtual ~StateProcessComponent();

        auto& OSSIAProcess() const
        { return m_ossia_process; }

    protected:
        StateElement& m_parent_state;
        std::shared_ptr<OSSIA::StateElement> m_ossia_process;
};

template<typename Process_T>
using StateProcessComponent_T = Scenario::GenericProcessComponent_T<StateProcessComponent, Process_T>;

class ISCORE_PLUGIN_OSSIA_EXPORT StateProcessComponentFactory :
        public iscore::GenericComponentFactory<
            Process::StateProcess,
            RecreateOnPlay::DocumentPlugin,
            RecreateOnPlay::StateProcessComponentFactory>
{
        ISCORE_ABSTRACT_FACTORY("cef1b394-84b2-4241-b4eb-72b1fb504f92")
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

template<typename StateProcessComponent_T>
class StateProcessComponentFactory_T :
        public iscore::GenericComponentFactoryImpl<StateProcessComponent_T, StateProcessComponentFactory>
{
    public:
        using model_type = typename StateProcessComponent_T::model_type;
        StateProcessComponent_T* make(
                  StateElement& st,
                  Process::StateProcess& proc,
                  const Context& ctx,
                  const Id<iscore::Component>& id,
                  QObject* parent) const override
        {
            return new StateProcessComponent_T{
                st, static_cast<model_type&>(proc), ctx, id, parent};
        }

        std::shared_ptr<OSSIA::StateElement> make(
                  Process::StateProcess& proc,
                  const Context& ctx) const override
        {
            return StateProcessComponent_T::make(proc, ctx);
        }
};

using StateProcessComponentFactoryList =
    iscore::GenericComponentFactoryList<
            Process::StateProcess,
            RecreateOnPlay::DocumentPlugin,
            RecreateOnPlay::StateProcessComponentFactory>;
}
