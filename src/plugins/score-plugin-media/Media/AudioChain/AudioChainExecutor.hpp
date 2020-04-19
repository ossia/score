#pragma once
#include <Media/AudioChain/AudioChainModel.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionContext.hpp>

#include <score/model/ComponentFactory.hpp>
#include <score/model/ComponentHierarchy.hpp>
#include <score/plugins/ModelFactory.hpp>

#include <ossia/dataflow/fx_node.hpp>
#include <ossia/dataflow/node_chain_process.hpp>
namespace Media
{

class AudioChainComponentBase : public ::Execution::ProcessComponent_T<
                                       Media::AudioChain::ProcessModel,
                                       ossia::node_chain_process>

{
  COMPONENT_METADATA("d638adb3-64da-4b6e-b84d-7c32684fa79d")
public:
  using parent_t = Execution::Component;
  using model_t = Process::ProcessModel;
  using component_t = ::Execution::ProcessComponent;
  using component_factory_list_t = Execution::ProcessComponentFactoryList;
  AudioChainComponentBase(
      Media::AudioChain::ProcessModel& element,
      const ::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  ~AudioChainComponentBase() override;

  ::Execution::ProcessComponent* make(
      const Id<score::Component>& id,
      Execution::ProcessComponentFactory& factory,
      Process::ProcessModel& process);

  ::Execution::ProcessComponent*
  make(const Id<score::Component>& id, Process::ProcessModel& process)
  {
    return nullptr;
  }
  void added(::Execution::ProcessComponent& e);

  std::function<void()>
  removing(const Process::ProcessModel& e, ::Execution::ProcessComponent& c);
  template <typename Component_T, typename Element, typename Fun>
  void removed(const Element& elt, const Component_T& comp, Fun f)
  {
    if (f)
      f();
  }
  void on_orderChanged();

  template <typename Models>
  auto& models() const
  {
    static_assert(
        std::is_same<Models, Process::ProcessModel>::value,
        "Effect component must be passed Process::EffectModel as child.");

    return process().effects();
  }

  struct RegisteredEffect
  {
    std::shared_ptr<::Execution::ProcessComponent> comp;

    Process::Inlets registeredInlets;
    Process::Outlets registeredOutlets;

    const auto& node() const { return comp->node; }
    operator bool() const { return bool(comp); }
  };

protected:
#if !defined(NDEBUG)
  bool m_clearing = false;
#endif

  std::vector<std::pair<Id<Process::ProcessModel>, RegisteredEffect>> m_fxes;

  void unreg(const RegisteredEffect& fx);
  void
  reg(const RegisteredEffect& fx, Execution::Transaction&);
  void unregister_old_first_node(
      std::pair<
          Id<Process::ProcessModel>,
          AudioChainComponentBase::RegisteredEffect>& new_first,
      Execution::Transaction& commands);
  void register_new_first_node(
      std::pair<
          Id<Process::ProcessModel>,
          AudioChainComponentBase::RegisteredEffect>& new_first,
      Execution::Transaction& commands);
  void unregister_old_last_node(
      std::pair<
          Id<Process::ProcessModel>,
          AudioChainComponentBase::RegisteredEffect>& new_first,
      Execution::Transaction& commands);
  void register_new_last_node(
      std::pair<
          Id<Process::ProcessModel>,
          AudioChainComponentBase::RegisteredEffect>& new_first,
      Execution::Transaction& commands);

  void register_node_again(
      std::pair<
          Id<Process::ProcessModel>,
          AudioChainComponentBase::RegisteredEffect>& new_first,
      Execution::Transaction& commands);
  void createPassthrough(Execution::Transaction&);
  void removePassthrough(Execution::Transaction&);

  std::shared_ptr<ossia::graph_node> m_passthrough{};
};

class AudioChainComponent final
    : public score::
          PolymorphicComponentHierarchy<AudioChainComponentBase, false>
{
public:
  AudioChainComponent(
      Media::AudioChain::ProcessModel& element,
      const ::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  void cleanup() override;

  AudioChainComponent(const AudioChainComponent&) = delete;
  AudioChainComponent(AudioChainComponent&&) = delete;
  AudioChainComponent& operator=(const AudioChainComponent&) = delete;
  AudioChainComponent& operator=(AudioChainComponent&&) = delete;
  ~AudioChainComponent();
};

using AudioChainComponentFactory
    = ::Execution::ProcessComponentFactory_T<AudioChainComponent>;
}

SCORE_CONCRETE_COMPONENT_FACTORY(
    Execution::ProcessComponentFactory,
    Execution::EffectComponentFactory)
