#pragma once

#include <score/model/ComponentFactory.hpp>
#include <score/model/ComponentHierarchy.hpp>
#include <score/plugins/customfactory/ModelFactory.hpp>
#include <Media/Effect/EffectProcessModel.hpp>
#include <Media/Effect/Effect/EffectComponent.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <ossia/dataflow/fx_node.hpp>

namespace Engine
{
namespace Execution
{


class SCORE_PLUGIN_ENGINE_EXPORT EffectComponent
    : public Media::Effect::GenericEffectComponent<const Context>
{
  Q_OBJECT
  ABSTRACT_COMPONENT_METADATA(
      Engine::Execution::EffectComponent,
      "ef902817-d181-479d-99e9-a120da35c544")

public:
    static constexpr bool is_unique = true;

  EffectComponent(
      Media::Effect::EffectModel& proc,
      const Context& ctx,
      const Id<score::Component>& id,
      QObject* parent)
      : Media::Effect::GenericEffectComponent<const Context>{proc, ctx, id, "ExecutorComponent", parent}
  {
  }

  virtual ~EffectComponent();

  std::shared_ptr<ossia::audio_fx_node> node;
};

template <typename Effect_T>
using EffectComponent_T
    = Media::Effect::GenericEffectComponent_T<EffectComponent, Effect_T>;


class SCORE_PLUGIN_ENGINE_EXPORT EffectComponentFactory
    : public score::GenericComponentFactory<Media::Effect::EffectModel, Engine::Execution::DocumentPlugin, Engine::Execution::EffectComponentFactory>
{
  SCORE_ABSTRACT_COMPONENT_FACTORY(Engine::Execution::EffectComponent)
public:
  virtual ~EffectComponentFactory() override;
  virtual std::shared_ptr<EffectComponent> make(
      Media::Effect::EffectModel& proc,
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
      Media::Effect::EffectModel& proc,
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
    GenericComponentFactoryList<Media::Effect::EffectModel, Engine::Execution::DocumentPlugin, Engine::Execution::EffectComponentFactory>
{
public:
  ~EffectComponentFactoryList();
};


struct effect_chain_process final :
    public ossia::time_process
{
    void
    state(ossia::time_value parent_date, double relative_position, ossia::time_value tick_offset, double speed) override
    {
      const ossia::token_request tk{parent_date, relative_position, tick_offset, speed};
      //startnode->requested_tokens.push_back(tk);
      //endnode->requested_tokens.push_back(tk);
      for(auto& node : nodes)
      {
        node->requested_tokens.push_back(tk);
      }
    }

    void stop() override
    {
      for(auto& node : nodes)
      {
        node->all_notes_off();
      }
    }
    std::vector<std::shared_ptr<ossia::audio_fx_node>> nodes;
};

class SCORE_PLUGIN_ENGINE_EXPORT EffectProcessComponentBase
    : public ::Engine::Execution::
    ProcessComponent_T<Media::Effect::ProcessModel, effect_chain_process>
{
  COMPONENT_METADATA("d638adb3-64da-4b6e-b84d-7c32684fa79d")
public:
    using parent_t = Engine::Execution::Component;
    using model_t = Media::Effect::EffectModel;
    using component_t = EffectComponent;
    using component_factory_list_t = EffectComponentFactoryList;
  EffectProcessComponentBase(
      Media::Effect::ProcessModel& element,
      const ::Engine::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  void recompute();

  ~EffectProcessComponentBase() override;


  EffectComponent* make(
          const Id<score::Component> & id,
          EffectComponentFactory& factory,
          Media::Effect::EffectModel &process);
  void added(EffectComponent& e);

  std::function<void()> removing(
      const Media::Effect::EffectModel& e,
      EffectComponent& c);
  template <typename Component_T, typename Element, typename Fun>
  void removed(const Element& elt, const Component_T& comp, Fun f)
  {
    if(f)
      f();
  }

  template <typename Models>
  auto& models() const
  {
    static_assert(
        std::is_same<Models, Media::Effect::EffectModel>::value,
        "Effect component must be passed Effect::EffectModel as child.");

    return process().effects();
  }

private:
  struct RegisteredEffect
  {
      std::shared_ptr<EffectComponent> comp;

      Process::Inlets registeredInlets;
      Process::Outlets registeredOutlets;

      const auto& node() const { return comp->node; }
      operator bool() const { return bool(comp); }
  };
  std::vector<std::pair<Id<Media::Effect::EffectModel>, RegisteredEffect>> m_fxes;


  void unreg(const RegisteredEffect& fx);
  void reg(const RegisteredEffect& fx);
};


class SCORE_PLUGIN_ENGINE_EXPORT EffectProcessComponent final :
        public score::PolymorphicComponentHierarchy<EffectProcessComponentBase, false>
{
    public:
  template<typename... Args>
  EffectProcessComponent(Args&&... args):
    score::PolymorphicComponentHierarchy<EffectProcessComponentBase, false>{
      score::lazy_init_t{}, std::forward<Args>(args)...}
  {
    init_hierarchy();
  }

  void cleanup() override;

  EffectProcessComponent(const EffectProcessComponent&) = delete;
  EffectProcessComponent(EffectProcessComponent&&) = delete;
  EffectProcessComponent& operator=(const EffectProcessComponent&) = delete;
  EffectProcessComponent& operator=(EffectProcessComponent&&) = delete;
  ~EffectProcessComponent();
};

using EffectProcessComponentFactory
= ::Engine::Execution::ProcessComponentFactory_T<EffectProcessComponent>;
}
}


SCORE_CONCRETE_COMPONENT_FACTORY(
    Engine::Execution::ProcessComponentFactory,
    Engine::Execution::EffectComponentFactory)
