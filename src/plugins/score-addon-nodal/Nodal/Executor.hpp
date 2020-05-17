#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <score/model/ComponentHierarchy.hpp>

#include <ossia/dataflow/node_process.hpp>
#include <ossia/detail/hash_map.hpp>

#include <Nodal/Process.hpp>
namespace Nodal
{
class NodalExecutorBase : public Execution::ProcessComponent_T<Nodal::Model, ossia::node_process>
{
  COMPONENT_METADATA("e85e0114-2a7e-4569-8a1d-f00c9fd22960")
public:
  using parent_t = Execution::Component;
  using model_t = Process::ProcessModel;
  using component_t = ::Execution::ProcessComponent;
  using component_factory_list_t = Execution::ProcessComponentFactoryList;

  NodalExecutorBase(
      Model& element,
      const Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  ~NodalExecutorBase();

  struct RegisteredNode
  {
    std::shared_ptr<Execution::ProcessComponent> comp;
  };

  ossia::fast_hash_map<Id<Process::ProcessModel>, RegisteredNode> m_nodes;

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

  std::function<void()> removing(const Process::ProcessModel& e, ::Execution::ProcessComponent& c);

  template <typename Component_T, typename Element, typename Fun>
  void removed(const Element& elt, const Component_T& comp, Fun f)
  {
    if (f)
      f();
  }

  template <typename Models>
  auto& models() const
  {
    static_assert(
        std::is_same<Models, Process::ProcessModel>::value,
        "Node component must be passed Process::ProcessModel as child.");

    return process().nodes;
  }

private:
  void reg(const RegisteredNode& fx, Execution::Transaction& vec);
  void unreg(const RegisteredNode& fx);
};

class HierarchyManager : public NodalExecutorBase, public Nano::Observer
{
public:
  using ParentComponent_T = NodalExecutorBase;
  using ChildModel_T = Process::ProcessModel;
  using ChildComponent_T = ::Execution::ProcessComponent;
  using ChildComponentFactoryList_T = Execution::ProcessComponentFactoryList;
  using hierarchy_t = HierarchyManager;

  struct ChildPair
  {
    ChildPair(ChildModel_T* m, ChildComponent_T* c) : model{m}, component{c} { }
    ChildModel_T* model{};
    ChildComponent_T* component{};
  };

  template <typename... Args>
  HierarchyManager(Args&&... args)
      : ParentComponent_T{std::forward<Args>(args)...}
      , m_componentFactory{
            score::AppComponents().template interfaces<ChildComponentFactoryList_T>()}
  {
    init_hierarchy();
  }

  void init_hierarchy()
  {
    auto& child_models = process().nodes;
    for (auto& child_model : child_models)
    {
      add(child_model);
    }

    child_models.mutable_added.template connect<&hierarchy_t::add>(this);

    child_models.removing.template connect<&hierarchy_t::remove>(this);
  }

  const auto& children() const { return m_children; }

  void add(Process::ProcessModel& model)
  {
    // Will return a factory for the given process if available
    auto id = getStrongId(model.components());
    if (auto factory = m_componentFactory.factory(model))
    {
      // The subclass should provide this function to construct
      // the correct component relative to this process.
      auto comp = this->make(id, *factory, model);
      if (comp)
      {
        model.components().add(comp);
        m_children.emplace_back(ChildPair{&model, comp});
        this->added(*comp);
      }
    }
    else
    {
      auto comp = ParentComponent_T::make(id, model);
      if (comp)
      {
        model.components().add(comp);
        m_children.emplace_back(ChildPair{&model, comp});
        ParentComponent_T::added(*comp);
      }
    }
  }

  void remove(const ChildModel_T& model)
  {
    auto it = ossia::find_if(m_children, [&](auto pair) { return pair.model == &model; });

    if (it != m_children.end())
    {
      do_cleanup(*it);
      m_children.erase(it);
    }
  }

  void clear()
  {
    for (const auto& element : m_children)
    {
      do_cleanup(element);
    }
    m_children.clear();
  }

  ~HierarchyManager() { clear(); }

private:
  void do_cleanup(const ChildPair& pair)
  {
    auto t = ParentComponent_T::removing(*pair.model, *pair.component);
    pair.model->components().erase(*pair.component);
    ParentComponent_T::removed(*pair.model, *pair.component, std::move(t));
  }

  const ChildComponentFactoryList_T& m_componentFactory;

  std::vector<ChildPair> m_children; // todo map ? multi_index with both index
                                     // of the component and of the process ?
};

class NodalExecutor final : public HierarchyManager
{
public:
  NodalExecutor(
      Nodal::Model& element,
      const ::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent)
      : HierarchyManager{element, ctx, id, parent}
  {
    // TODO passthrough ?
  }

  void cleanup() override;

  NodalExecutor(const NodalExecutor&) = delete;
  NodalExecutor(NodalExecutor&&) = delete;
  NodalExecutor& operator=(const NodalExecutor&) = delete;
  NodalExecutor& operator=(NodalExecutor&&) = delete;
  ~NodalExecutor();
};

using ProcessExecutorComponentFactory = Execution::ProcessComponentFactory_T<NodalExecutor>;
}
