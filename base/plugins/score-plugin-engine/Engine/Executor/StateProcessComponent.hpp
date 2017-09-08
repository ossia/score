#pragma once
#include <Process/StateProcess.hpp>

#include <score/model/Component.hpp>
#include <score/model/ComponentFactory.hpp>

#include <score_plugin_engine_export.h>

#include <ossia/editor/state/state_element.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/StateComponent.hpp>
#include <Scenario/Document/Components/ProcessComponent.hpp>
#include <score/plugins/customfactory/ModelFactory.hpp>
namespace Engine
{
namespace Execution
{
class SCORE_PLUGIN_ENGINE_EXPORT StateProcessComponent
    : public Process::GenericStateProcessComponent<const Context>
{
  ABSTRACT_COMPONENT_METADATA(
      StateProcessComponent, "cef1b394-84b2-4241-b4eb-72b1fb504f92")
public:
  StateProcessComponent(
      StateComponent& state,
      Process::StateProcess& proc,
      const Context& ctx,
      const Id<score::Component>& id,
      const QString& name,
      QObject* parent)
      : Process::GenericStateProcessComponent<const Context>{proc, ctx, id,
                                                              name, nullptr}
      , m_parent_state{state}
  {
  }

  virtual ~StateProcessComponent();

  auto& OSSIAState() const
  {
    return m_ossia_state;
  }

protected:
  StateComponent& m_parent_state;
  ossia::state_element m_ossia_state;
};

template <typename Process_T>
using StateProcessComponent_T
    = Process::GenericProcessComponent_T<StateProcessComponent, Process_T>;

class SCORE_PLUGIN_ENGINE_EXPORT StateProcessComponentFactory
    : public score::
          GenericComponentFactory<Process::StateProcess, Engine::Execution::DocumentPlugin, Engine::Execution::StateProcessComponentFactory>
{
  SCORE_ABSTRACT_COMPONENT_FACTORY(StateProcessComponent)
public:
  virtual ~StateProcessComponentFactory();

  virtual StateProcessComponent* make(
      StateComponent& cst,
      Process::StateProcess& proc,
      const Context& ctx,
      const Id<score::Component>& id,
      QObject* parent) const = 0;

  virtual ossia::state_element
  make(Process::StateProcess& proc, const Context& ctxt) const = 0;
};

template <typename StateProcessComponent_T>
class StateProcessComponentFactory_T
    : public score::
          GenericComponentFactoryImpl<StateProcessComponent_T, StateProcessComponentFactory>
{
public:
  using model_type = typename StateProcessComponent_T::model_type;
  StateProcessComponent_T* make(
      StateComponent& st,
      Process::StateProcess& proc,
      const Context& ctx,
      const Id<score::Component>& id,
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

class SCORE_PLUGIN_ENGINE_EXPORT StateProcessComponentFactoryList final : public score::
    GenericComponentFactoryList<Process::StateProcess, Engine::Execution::DocumentPlugin, Engine::Execution::StateProcessComponentFactory>
{
public:
  ~StateProcessComponentFactoryList();
};
}
}
