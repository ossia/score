#pragma once
#include <ossia/detail/algorithms.hpp>

#include <nano_observer.hpp>
#include <score/model/ComponentSerialization.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <vector>

namespace score
{
/**
 * \class ComponentHierarchyManager
 * \brief Manages simple hierarchies of components
 *
 * For instance, if the object Foo is a container for Bar elements,
 * this class can be used so that every time a new Bar is created, a matching
 * component will be added to Bar.
 *
 * \todo If the number of such hierarchies grows, it may be interesting instead
 * to store them in a single hierarchy manager part of the original element.
 */
template <
    typename ParentComponent_T,
    typename ChildModel_T,
    typename ChildComponent_T>
class ComponentHierarchyManager
    : public ParentComponent_T
    , public Nano::Observer
{
public:
  using hierarchy_t = ComponentHierarchyManager;

  struct ChildPair
  {
    ChildPair(ChildModel_T* m, ChildComponent_T* c) : model{m}, component{c}
    {
    }
    ChildModel_T* model{};
    ChildComponent_T* component{};
  };

  template <typename... Args>
  ComponentHierarchyManager(Args&&... args)
      : ParentComponent_T{std::forward<Args>(args)...}
  {
    init();
  }

  template <typename... Args>
  ComponentHierarchyManager(score::lazy_init_t, Args&&... args)
      : ParentComponent_T{std::forward<Args>(args)...}
  {
  }

  void init()
  {
    auto& child_models = ParentComponent_T::template models<ChildModel_T>();
    for (auto& child_model : child_models)
    {
      add(child_model);
    }

    child_models.mutable_added
        .template connect<&hierarchy_t::add>(this);

    child_models.removing.template connect<&hierarchy_t::remove>(
        this);
  }

  const auto& children() const
  {
    return m_children;
  }

  void add(ChildModel_T& element)
  {
    add(element,
        typename score::is_component_serializable<ChildComponent_T>::type{});
  }

  void add(ChildModel_T& element, score::serializable_tag)
  {
    // Since the component may be serializable, we first look if
    // we can deserialize it.
    auto comp = score::deserialize_component<ChildComponent_T>(
        element.components(), [&](auto&& deserializer) {
          ParentComponent_T::template load<ChildComponent_T>(
              deserializer, element);
        });

    // Maybe we could not deserialize it
    if (!comp)
    {
      comp = ParentComponent_T::template make<ChildComponent_T>(
          getStrongId(element.components()), element);
    }

    // We try to add it
    if (comp)
    {
      element.components().add(comp);
      m_children.emplace_back(ChildPair{&element, comp});
    }
  }

  void add(ChildModel_T& model, score::not_serializable_tag)
  {
    // The subclass should provide this function to construct
    // the correct component relative to this process.
    auto proc_comp
        = ParentComponent_T::make(getStrongId(model.components()), model);
    if (proc_comp)
    {
      model.components().add(proc_comp);
      m_children.emplace_back(ChildPair{&model, proc_comp});
    }
  }

  void remove(const ChildModel_T& model)
  {
    auto it = ossia::find_if(
        m_children, [&](auto pair) { return pair.model == &model; });

    if (it != m_children.end())
    {
      cleanup(*it);
      m_children.erase(it);
    }
  }

  void cleanup(const ChildPair& pair)
  {
    ParentComponent_T::removing(*pair.model, *pair.component);
    pair.model->components().remove(*pair.component);
  }

  void clear()
  {
    for (const auto& element : m_children)
    {
      cleanup(element);
    }
    m_children.clear();
  }

  ~ComponentHierarchyManager()
  {
    clear();
  }

private:
  std::vector<ChildPair> m_children; // todo map ? multi_index with both index
                                     // of the component and of the process ?
};

/**
 * \class PolymorphicComponentHierarchyManager
 * \brief Manages polymorphic hierarchies of components
 *
 * Like ComponentHierarchyManager, but used when the components of the child
 * class are polymorphic. For instance, if we have a Process, we want to create
 * components that will be specific to each Process type. But then we have to
 * source them from a factory somehow.
 */
template <
    typename ParentComponent_T,
    typename ChildModel_T,
    typename ChildComponent_T,
    typename ChildComponentFactoryList_T,
    bool HasOwnership = true>
class PolymorphicComponentHierarchyManager
    : public ParentComponent_T
    , public Nano::Observer
{
public:
  using hierarchy_t = PolymorphicComponentHierarchyManager;

  struct ChildPair
  {
    ChildPair(ChildModel_T* m, ChildComponent_T* c) : model{m}, component{c}
    {
    }
    ChildModel_T* model{};
    ChildComponent_T* component{};
  };

  template <typename... Args>
  PolymorphicComponentHierarchyManager(Args&&... args)
      : ParentComponent_T{std::forward<Args>(args)...}
      , m_componentFactory{
            score::AppComponents()
                .template interfaces<ChildComponentFactoryList_T>()}
  {
    init_hierarchy();
  }

  template <typename... Args>
  PolymorphicComponentHierarchyManager(lazy_init_t, Args&&... args)
      : ParentComponent_T{std::forward<Args>(args)...}
      , m_componentFactory{
            score::AppComponents()
                .template interfaces<ChildComponentFactoryList_T>()}
  {
  }

  void init_hierarchy()
  {
    auto& child_models = ParentComponent_T::template models<ChildModel_T>();
    for (auto& child_model : child_models)
    {
      add(child_model);
    }

    child_models.mutable_added
        .template connect<&hierarchy_t::add>(this);

    child_models.removing.template connect<&hierarchy_t::remove>(
        this);
  }
  const auto& children() const
  {
    return m_children;
  }

  void add(ChildModel_T& element)
  {
    add_impl(
        element,
        typename score::is_component_serializable<ChildComponent_T>::type{});
  }

  void remove(const ChildModel_T& model)
  {
    auto it = ossia::find_if(
        m_children, [&](auto pair) { return pair.model == &model; });

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

  ~PolymorphicComponentHierarchyManager()
  {
    clear();
  }

private:
  // TODO remove these useless templates when MSVC grows some brains
  template <typename TheChild>
  void add_impl(TheChild& model, score::serializable_tag)
  {
    // Will return a factory for the given process if available
    if (auto factory = m_componentFactory.factory(model))
    {
      // Since the component may be serializable, we first look if
      // we can deserialize it.
      ChildComponent_T* comp = score::deserialize_component<ChildComponent_T>(
          model.components(), [&](auto&& deserializer) {
            ParentComponent_T::template load<ChildComponent_T>(
                deserializer, *factory, model);
          });

      // Maybe we could not deserialize it
      if (!comp)
      {
        comp = ParentComponent_T::template make<ChildComponent_T>(
            getStrongId(model.components()), *factory, model);
      }

      // We try to add it
      if (comp)
      {
        model.components().add(comp);
        m_children.emplace_back(ChildPair{&model, comp});
        ParentComponent_T::added(*comp);
      }
    }
  }

  template <typename TheChild>
  void add_impl(TheChild& model, score::not_serializable_tag)
  {
    // Will return a factory for the given process if available
    if (auto factory = m_componentFactory.factory(model))
    {
      // The subclass should provide this function to construct
      // the correct component relative to this process.
      auto comp = ParentComponent_T::make(
          getStrongId(model.components()), *factory, model);
      if (comp)
      {
        model.components().add(comp);
        m_children.emplace_back(ChildPair{&model, comp});
        ParentComponent_T::added(*comp);
      }
    }
  }

  void do_cleanup(const ChildPair& pair)
  {
    if constexpr (HasOwnership)
    {
      ParentComponent_T::removing(*pair.model, *pair.component);
      pair.model->components().remove(*pair.component);
    }
    else
    {
      auto t = ParentComponent_T::removing(*pair.model, *pair.component);
      pair.model->components().erase(*pair.component);
      ParentComponent_T::removed(*pair.model, *pair.component, std::move(t));
    }
  }

  const ChildComponentFactoryList_T& m_componentFactory;

  std::vector<ChildPair> m_children; // todo map ? multi_index with both index
                                     // of the component and of the process ?
};

template <typename Component>
using ComponentHierarchy = ComponentHierarchyManager<
    Component,
    typename Component::model_t,
    typename Component::component_t>;

template <typename Component, bool HasOwnership = true>
using PolymorphicComponentHierarchy = PolymorphicComponentHierarchyManager<
    Component,
    typename Component::model_t,
    typename Component::component_t,
    typename Component::component_factory_list_t,
    HasOwnership>;
}
