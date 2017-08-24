#pragma once
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <iscore/model/Component.hpp>
#include <ossia/detail/algorithms.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <iscore/model/ComponentSerialization.hpp>


template <
    typename Component_T,
    typename Scenario_T,
    typename ConstraintComponent_T,
    bool HasOwnership = true>
class SimpleHierarchicalScenarioComponent : public Component_T, public Nano::Observer
{
public:
  struct ConstraintPair
  {
    using element_t = Scenario::ConstraintModel;
    Scenario::ConstraintModel& element;
    ConstraintComponent_T& component;
  };

  //! The default constructor will also initialize the children
  template <typename... Args>
  SimpleHierarchicalScenarioComponent(Args&&... args)
      : Component_T{std::forward<Args>(args)...}
  {
    init();
  }


  //! This constructor allows for initializing the children later. Useful for std::enable_shared_from_this.
  template <typename... Args>
  SimpleHierarchicalScenarioComponent(iscore::lazy_init_t, Args&&... args)
      : Component_T{std::forward<Args>(args)...}
  {
  }

  //! Do not forget to call this when using the lazy constructor.
  void init()
  {
    setup<Scenario::ConstraintModel>();
  }

  const std::list<ConstraintPair>& constraints_pairs() const
  {
    return m_constraints;
  }

  void clear()
  {
    for (auto element : m_constraints)
      do_cleanup(element);

    m_constraints.clear();
  }

  ~SimpleHierarchicalScenarioComponent()
  {
    clear();
  }

  template <typename elt_t>
  void remove(const elt_t& element)
  {
    using map_t = MatchingComponent<elt_t, true>;
    auto& container = this->*map_t::local_container;

    auto it = ossia::find_if(
        container, [&](auto pair) { return &pair.element == &element; });

    if (it != container.end())
    {
      do_cleanup(*it);
      container.erase(it);
    }
  }

private:
  template <typename T, bool dummy = true>
  struct MatchingComponent;

  template <typename Pair_T>
  void do_cleanup(const Pair_T& pair)
  {
    // TODO constexpr-if
    if(HasOwnership)
    {
      Component_T::removing(pair.element, pair.component);
      pair.element.components().remove(pair.component);
    }
    else
    {
      auto t = Component_T::removing(pair.element, pair.component);
      pair.element.components().erase(pair.component);
      Component_T::removed(pair.element, pair.component, std::move(t));
    }
  }

  template <typename elt_t>
  void setup()
  {
    using map_t = MatchingComponent<elt_t, true>;
    auto&& member = map_t::scenario_container(Component_T::process());

    for (auto& elt : member)
    {
      add(elt);
    }

    member.mutable_added
        .template connect<SimpleHierarchicalScenarioComponent, &SimpleHierarchicalScenarioComponent::add>(
            this);

    member.removing
        .template connect<SimpleHierarchicalScenarioComponent, &SimpleHierarchicalScenarioComponent::remove>(
            this);
  }

  template <typename elt_t>
  void add(elt_t& element)
  {
    add(element,
        typename iscore::is_component_serializable<
          typename MatchingComponent<elt_t, true>::type
        >::type{});
  }

  template <typename elt_t>
  void add(elt_t& element, iscore::serializable_tag)
  {
    using map_t = MatchingComponent<elt_t, true>;
    using component_t = typename map_t::type;

    // Since the component may be serializable, we first look if
    // we can deserialize it.
    auto comp = iscore::deserialize_component<component_t>(
          element.components(),
          [&] (auto&& deserializer) {
      Component_T::template load<component_t>(deserializer, element);
    });

    // Maybe we could not deserialize it
    if(!comp)
    {
      comp = Component_T::template make<component_t>(
            getStrongId(element.components()), element);
    }

    // We try to add it
    if (comp)
    {
      element.components().add(comp);
      (this->*map_t::local_container)
          .emplace_back(typename map_t::pair_type{element, *comp});
    }
  }

  template <typename elt_t>
  void add(elt_t& element, iscore::not_serializable_tag)
  {
    // We can just create a new component directly
    using map_t = MatchingComponent<elt_t, true>;
    auto comp = Component_T::make(
        getStrongId(element.components()), element);
    if (comp)
    {
      element.components().add(comp);
      (this->*map_t::local_container)
          .emplace_back(typename map_t::pair_type{element, *comp});
    }
  }

  std::list<ConstraintPair> m_constraints;

  template <bool dummy>
  struct MatchingComponent<Scenario::ConstraintModel, dummy>
  {
    using type = ConstraintComponent_T;
    using pair_type = ConstraintPair;
    static const constexpr auto local_container
        = &SimpleHierarchicalScenarioComponent::m_constraints;
    static const constexpr auto scenario_container = Scenario::
        ElementTraits<Scenario_T, Scenario::ConstraintModel>::accessor;
  };
};



template <
    typename Component_T,
    typename Scenario_T,
    typename ConstraintComponent_T,
    typename EventComponent_T,
    typename TimeSyncComponent_T,
    typename StateComponent_T,
    bool HasOwnership = true>
class HierarchicalScenarioComponent : public Component_T, public Nano::Observer
{
public:
  struct ConstraintPair
  {
    using element_t = Scenario::ConstraintModel;
    Scenario::ConstraintModel& element;
    ConstraintComponent_T& component;
  };
  struct EventPair
  {
    using element_t = Scenario::EventModel;
    Scenario::EventModel& element;
    EventComponent_T& component;
  };
  struct TimeSyncPair
  {
    using element_t = Scenario::TimeSyncModel;
    Scenario::TimeSyncModel& element;
    TimeSyncComponent_T& component;
  };
  struct StatePair
  {
    using element_t = Scenario::StateModel;
    Scenario::StateModel& element;
    StateComponent_T& component;
  };

  //! The default constructor will also initialize the children
  template <typename... Args>
  HierarchicalScenarioComponent(Args&&... args)
      : Component_T{std::forward<Args>(args)...}
  {
    init();
  }


  //! This constructor allows for initializing the children later. Useful for std::enable_shared_from_this.
  template <typename... Args>
  HierarchicalScenarioComponent(iscore::lazy_init_t, Args&&... args)
      : Component_T{std::forward<Args>(args)...}
  {
  }

  //! Do not forget to call this when using the lazy constructor.
  void init()
  {
    setup<Scenario::TimeSyncModel>();
    setup<Scenario::EventModel>();
    setup<Scenario::StateModel>();
    setup<Scenario::ConstraintModel>();
  }

  const std::list<ConstraintPair>& constraints_pairs() const
  {
    return m_constraints;
  }
  const std::list<EventPair>& events_pairs() const
  {
    return m_events;
  }
  const std::list<StatePair>& states_pairs() const
  {
    return m_states;
  }
  const std::list<TimeSyncPair>& timeSyncs_pairs() const
  {
    return m_timeSyncs;
  }

  void clear()
  {
    for (auto element : m_constraints)
      do_cleanup(element);
    for (auto element : m_states)
      do_cleanup(element);
    for (auto element : m_events)
      do_cleanup(element);
    for (auto element : m_timeSyncs)
      do_cleanup(element);

    m_constraints.clear();
    m_states.clear();
    m_events.clear();
    m_timeSyncs.clear();
  }

  ~HierarchicalScenarioComponent()
  {
    clear();
  }

  template <typename elt_t>
  void remove(const elt_t& element)
  {
    using map_t = MatchingComponent<elt_t, true>;
    auto& container = this->*map_t::local_container;

    auto it = ossia::find_if(
        container, [&](auto pair) { return &pair.element == &element; });

    if (it != container.end())
    {
      do_cleanup(*it);
      container.erase(it);
    }
  }

private:
  template <typename T, bool dummy = true>
  struct MatchingComponent;

  template <typename Pair_T>
  void do_cleanup(const Pair_T& pair)
  {
    // TODO constexpr-if
    if(HasOwnership)
    {
      Component_T::removing(pair.element, pair.component);
      pair.element.components().remove(pair.component);
    }
    else
    {
      auto t = Component_T::removing(pair.element, pair.component);
      pair.element.components().erase(pair.component);
      Component_T::removed(pair.element, pair.component, std::move(t));
    }
  }

  template <typename elt_t>
  void setup()
  {
    using map_t = MatchingComponent<elt_t, true>;
    auto&& member = map_t::scenario_container(Component_T::process());

    for (auto& elt : member)
    {
      add(elt);
    }

    member.mutable_added
        .template connect<HierarchicalScenarioComponent, &HierarchicalScenarioComponent::add>(
            this);

    member.removing
        .template connect<HierarchicalScenarioComponent, &HierarchicalScenarioComponent::remove>(
            this);
  }

  template <typename elt_t>
  void add(elt_t& element)
  {
    add(element,
        typename iscore::is_component_serializable<
          typename MatchingComponent<elt_t, true>::type
        >::type{});
  }

  template <typename elt_t>
  void add(elt_t& element, iscore::serializable_tag)
  {
    using map_t = MatchingComponent<elt_t, true>;
    using component_t = typename map_t::type;

    // Since the component may be serializable, we first look if
    // we can deserialize it.
    auto comp = iscore::deserialize_component<component_t>(
          element.components(),
          [&] (auto&& deserializer) {
      Component_T::template load<component_t>(deserializer, element);
    });

    // Maybe we could not deserialize it
    if(!comp)
    {
      comp = Component_T::template make<component_t>(
            getStrongId(element.components()), element);
    }

    // We try to add it
    if (comp)
    {
      element.components().add(comp);
      (this->*map_t::local_container)
          .emplace_back(typename map_t::pair_type{element, *comp});
    }
  }

  template <typename elt_t>
  void add(elt_t& element, iscore::not_serializable_tag)
  {
    // We can just create a new component directly
    using map_t = MatchingComponent<elt_t, true>;
    auto comp = Component_T::template make<typename map_t::type>(
        getStrongId(element.components()), element);
    if (comp)
    {
      element.components().add(comp);
      (this->*map_t::local_container)
          .emplace_back(typename map_t::pair_type{element, *comp});
    }
  }

  std::list<TimeSyncPair> m_timeSyncs;
  std::list<EventPair> m_events;
  std::list<StatePair> m_states;
  std::list<ConstraintPair> m_constraints;

  template <bool dummy>
  struct MatchingComponent<Scenario::ConstraintModel, dummy>
  {
    using type = ConstraintComponent_T;
    using pair_type = ConstraintPair;
    static const constexpr auto local_container
        = &HierarchicalScenarioComponent::m_constraints;
    static const constexpr auto scenario_container = Scenario::
        ElementTraits<Scenario_T, Scenario::ConstraintModel>::accessor;
  };
  template <bool dummy>
  struct MatchingComponent<Scenario::EventModel, dummy>
  {
    using type = EventComponent_T;
    using pair_type = EventPair;
    static const constexpr auto local_container
        = &HierarchicalScenarioComponent::m_events;
    static const constexpr auto scenario_container = Scenario::
        ElementTraits<Scenario_T, Scenario::EventModel>::accessor;
  };
  template <bool dummy>
  struct MatchingComponent<Scenario::TimeSyncModel, dummy>
  {
    using type = TimeSyncComponent_T;
    using pair_type = TimeSyncPair;
    static const constexpr auto local_container
        = &HierarchicalScenarioComponent::m_timeSyncs;
    static const constexpr auto scenario_container = Scenario::
        ElementTraits<Scenario_T, Scenario::TimeSyncModel>::accessor;
  };
  template <bool dummy>
  struct MatchingComponent<Scenario::StateModel, dummy>
  {
    using type = StateComponent_T;
    using pair_type = StatePair;
    static const constexpr auto local_container
        = &HierarchicalScenarioComponent::m_states;
    static const constexpr auto scenario_container = Scenario::
        ElementTraits<Scenario_T, Scenario::StateModel>::accessor;
  };
};


template <
    typename Component_T,
    typename BaseScenario_T,
    typename ConstraintComponent_T,
    typename EventComponent_T,
    typename TimeSyncComponent_T,
    typename StateComponent_T>
class HierarchicalBaseScenario : public Component_T, public Nano::Observer
{
public:
  struct ConstraintPair
  {
    using element_t = Scenario::ConstraintModel;
    Scenario::ConstraintModel& element;
    ConstraintComponent_T& component;
  };
  struct EventPair
  {
    using element_t = Scenario::EventModel;
    Scenario::EventModel& element;
    EventComponent_T& component;
  };
  struct TimeSyncPair
  {
    using element_t = Scenario::TimeSyncModel;
    Scenario::TimeSyncModel& element;
    TimeSyncComponent_T& component;
  };
  struct StatePair
  {
    using element_t = Scenario::StateModel;
    Scenario::StateModel& element;
    StateComponent_T& component;
  };

  template <typename... Args>
  HierarchicalBaseScenario(Args&&... args)
      : Component_T{std::forward<Args>(args)...}
      , m_timeSyncs{setup<Scenario::TimeSyncModel>(0),
                    setup<Scenario::TimeSyncModel>(1)}
      , m_events{setup<Scenario::EventModel>(0),
                 setup<Scenario::EventModel>(1)}
      , m_states{setup<Scenario::StateModel>(0),
                 setup<Scenario::StateModel>(1)}
      , m_constraints{setup<Scenario::ConstraintModel>(0)}
  {
  }

  const auto& constraints() const
  {
    return m_constraints;
  }
  const auto& events() const
  {
    return m_events;
  }
  const auto& states() const
  {
    return m_states;
  }
  const auto& timeSyncs() const
  {
    return m_timeSyncs;
  }

  void clear()
  {
    for (auto element : m_constraints)
      cleanup(element);
    for (auto element : m_states)
      cleanup(element);
    for (auto element : m_events)
      cleanup(element);
    for (auto element : m_timeSyncs)
      cleanup(element);

    m_constraints.clear();
    m_states.clear();
    m_events.clear();
    m_timeSyncs.clear();
  }

  ~HierarchicalBaseScenario()
  {
    clear();
  }

private:
  template <typename T, bool dummy = true>
  struct MatchingComponent;

  template <typename Pair_T>
  void cleanup(const Pair_T& pair)
  {
    Component_T::removing(pair.element, pair.component);
    pair.element.components().remove(pair.component);
  }

  template <typename elt_t>
  auto setup(int pos)
  {
    using map_t = MatchingComponent<elt_t, true>;
    auto&& member = map_t::scenario_container(this->process());

    return add(member[pos], pos);
  }

  template <typename elt_t>
  auto add(elt_t& element, int pos)
  {
    using map_t = MatchingComponent<elt_t, true>;
    auto comp = Component_T::template make<typename map_t::type>(
        getStrongId(element.components()), element);

    ISCORE_ASSERT(comp);
    element.components().add(comp);
    return typename map_t::pair_type{element, *comp};
  }

  template <typename elt_t>
  void remove(const elt_t& element)
  {
    using map_t = MatchingComponent<elt_t, true>;
    auto& container = this->*map_t::local_container;

    auto it = find_if(
        container, [&](auto pair) { return &pair.element == &element; });

    if (it != container.end())
    {
      cleanup(*it);
      container.erase(it);
    }
  }

  std::list<TimeSyncPair> m_timeSyncs;
  std::list<EventPair> m_events;
  std::list<StatePair> m_states;
  std::list<ConstraintPair> m_constraints;

  template <bool dummy>
  struct MatchingComponent<Scenario::ConstraintModel, dummy>
  {
    using type = ConstraintComponent_T;
    using pair_type = ConstraintPair;
    static const constexpr auto local_container
        = &HierarchicalBaseScenario::m_constraints;
    static const constexpr auto scenario_container = Scenario::
        ElementTraits<BaseScenario_T, Scenario::ConstraintModel>::
            accessor;
  };
  template <bool dummy>
  struct MatchingComponent<Scenario::EventModel, dummy>
  {
    using type = EventComponent_T;
    using pair_type = EventPair;
    static const constexpr auto local_container
        = &HierarchicalBaseScenario::m_events;
    static const constexpr auto scenario_container = Scenario::
        ElementTraits<BaseScenario_T, Scenario::EventModel>::accessor;
  };
  template <bool dummy>
  struct MatchingComponent<Scenario::TimeSyncModel, dummy>
  {
    using type = TimeSyncComponent_T;
    using pair_type = TimeSyncPair;
    static const constexpr auto local_container
        = &HierarchicalBaseScenario::m_timeSyncs;
    static const constexpr auto scenario_container = Scenario::
        ElementTraits<BaseScenario_T, Scenario::TimeSyncModel>::
            accessor;
  };
  template <bool dummy>
  struct MatchingComponent<Scenario::StateModel, dummy>
  {
    using type = StateComponent_T;
    using pair_type = StatePair;
    static const constexpr auto local_container
        = &HierarchicalBaseScenario::m_states;
    static const constexpr auto scenario_container = Scenario::
        ElementTraits<BaseScenario_T, Scenario::StateModel>::accessor;
  };
};
