#pragma once
#include <Effect/EffectComponent.hpp>
#include <score/model/ComponentFactory.hpp>
#include <score/model/ComponentHierarchy.hpp>
#include <score/plugins/customfactory/ModelFactory.hpp>
#include <ossia/dataflow/fx_node.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <score_plugin_engine_export.h>
namespace Engine::Execution
{
class SCORE_PLUGIN_ENGINE_EXPORT EffectComponent
    : public Process::GenericEffectComponent<const Context>
{
  Q_OBJECT
  ABSTRACT_COMPONENT_METADATA(
      Engine::Execution::EffectComponent,
      "ef902817-d181-479d-99e9-a120da35c544")

public:
    static constexpr bool is_unique = true;

  EffectComponent(
      Process::EffectModel& proc,
      const Context& ctx,
      const Id<score::Component>& id,
      QObject* parent)
      : Process::GenericEffectComponent<const Context>{proc, ctx, id, "ExecutorComponent", parent}
  {
  }

  virtual ~EffectComponent();

  std::shared_ptr<ossia::audio_fx_node> node;
};

template <typename Effect_T>
using EffectComponent_T
    = Process::GenericEffectComponent_T<EffectComponent, Effect_T>;


class SCORE_PLUGIN_ENGINE_EXPORT EffectComponentFactory
    : public score::GenericComponentFactory<Process::EffectModel, Engine::Execution::DocumentPlugin, Engine::Execution::EffectComponentFactory>
{
  SCORE_ABSTRACT_COMPONENT_FACTORY(Engine::Execution::EffectComponent)
public:
  virtual ~EffectComponentFactory() override;
  virtual std::shared_ptr<EffectComponent> make(
      Process::EffectModel& proc,
      const Context& ctx,
      const Id<score::Component>& id,
      QObject* parent) const = 0;

  //! Reimplement this if the element needs two-phase initialization.
  virtual void init(EffectComponent* comp) const;
};

template <typename EffectComponent_T>
class EffectComponentFactory_T
    : public score::
          GenericComponentFactoryImpl<EffectComponent_T, EffectComponentFactory>
{
public:
  using model_type = typename EffectComponent_T::model_type;
  std::shared_ptr<EffectComponent> make(
      Process::EffectModel& proc,
      const Context& ctx,
      const Id<score::Component>& id,
      QObject* parent) const final override
  {
    auto comp = std::make_shared<EffectComponent_T>(static_cast<model_type&>(proc), ctx, id, parent);
    this->init(comp.get());
    return comp;
  }
};

class SCORE_PLUGIN_ENGINE_EXPORT EffectComponentFactoryList final : public score::
    GenericComponentFactoryList<Process::EffectModel, Engine::Execution::DocumentPlugin, Engine::Execution::EffectComponentFactory>
{
public:
  ~EffectComponentFactoryList();
};


}
