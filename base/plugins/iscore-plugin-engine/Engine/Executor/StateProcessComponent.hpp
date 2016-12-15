#pragma once
#include <Process/StateProcess.hpp>

#include <iscore/model/Component.hpp>
#include <iscore/model/ComponentFactory.hpp>

#include <iscore_plugin_engine_export.h>

#include <ossia/editor/state/state_element.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/StateElement.hpp>
#include <Scenario/Document/Components/ProcessComponent.hpp>
#include <iscore/plugins/customfactory/ModelFactory.hpp>
namespace Engine
{
namespace Execution
{
class ISCORE_PLUGIN_ENGINE_EXPORT StateProcessComponent
    : public Scenario::GenericStateProcessComponent<const Context>
{
  ABSTRACT_COMPONENT_METADATA(
      StateProcessComponent, "cef1b394-84b2-4241-b4eb-72b1fb504f92")
public:
  StateProcessComponent(
      StateElement& state,
      Process::StateProcess& proc,
      const Context& ctx,
      const Id<iscore::Component>& id,
      const QString& name,
      QObject* parent)
      : Scenario::GenericStateProcessComponent<const Context>{proc, ctx, id,
                                                              name, parent}
      , m_parent_state{state}
  {
  }

  virtual ~StateProcessComponent();

  auto& OSSIAState() const
  {
    return m_ossia_state;
  }

protected:
  StateElement& m_parent_state;
  ossia::state_element m_ossia_state;
};

template <typename Process_T>
using StateProcessComponent_T
    = Scenario::GenericProcessComponent_T<StateProcessComponent, Process_T>;

class ISCORE_PLUGIN_ENGINE_EXPORT StateProcessComponentFactory
    : public iscore::
          GenericComponentFactory<Process::StateProcess, Engine::Execution::DocumentPlugin, Engine::Execution::StateProcessComponentFactory>
{
  ISCORE_ABSTRACT_COMPONENT_FACTORY(StateProcessComponent)
public:
  virtual ~StateProcessComponentFactory();

  virtual StateProcessComponent* make(
      StateElement& cst,
      Process::StateProcess& proc,
      const Context& ctx,
      const Id<iscore::Component>& id,
      QObject* parent) const = 0;

  virtual ossia::state_element
  make(Process::StateProcess& proc, const Context& ctxt) const = 0;
};

template <typename StateProcessComponent_T>
class StateProcessComponentFactory_T
    : public iscore::
          GenericComponentFactoryImpl<StateProcessComponent_T, StateProcessComponentFactory>
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
    return new StateProcessComponent_T{st, static_cast<model_type&>(proc), ctx,
                                       id, parent};
  }

  ossia::state_element
  make(Process::StateProcess& proc, const Context& ctx) const override
  {
    return StateProcessComponent_T::make(proc, ctx);
  }
};

using StateProcessComponentFactoryList = iscore::
    GenericComponentFactoryList<Process::StateProcess, Engine::Execution::DocumentPlugin, Engine::Execution::StateProcessComponentFactory>;
}
}
