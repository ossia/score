#ifndef SCORE_INTERFACELIST_2018_10_22
#define SCORE_INTERFACELIST_2018_10_22
#pragma once
#include <score/plugins/Interface.hpp>
#include <score/tools/std/IndirectContainer.hpp>
#include <score/tools/std/Pointer.hpp>

#include <ossia/detail/hash_map.hpp>

#include <nano_signal_slot.hpp>
#include <score_lib_base_export.h>

namespace score
{
/**
 * @brief InterfaceListBase Interface to access factories.
 *
 * Actual instances are available through score::ApplicationContext:
 *
 * @code
 * auto& factories = context.interfaces<MyConcreteFactoryList>();
 * @endcode
 *
 * The interface lists are initialised first when scanning the plug-ins,
 * then all the factories are added once all the lists of all plug-ins are
 * instantiated.
 */
class SCORE_LIB_BASE_EXPORT InterfaceListBase
{
public:
  static constexpr bool factory_list_tag = true;
  InterfaceListBase() noexcept = default;
  InterfaceListBase(const InterfaceListBase&) = delete;
  InterfaceListBase(InterfaceListBase&&) = delete;
  InterfaceListBase& operator=(const InterfaceListBase&) = delete;
  InterfaceListBase& operator=(InterfaceListBase&&) = delete;
  virtual ~InterfaceListBase();

  //! A key that uniquely identifies this family of factories.
  virtual score::InterfaceKey interfaceKey() const noexcept = 0;

  /**
   * @brief insert Register a new factory.
   *
   * All the factories are registered upon loading.
   */
  virtual void insert(std::unique_ptr<score::InterfaceBase>) = 0;

  /**
   * @brief optimize Called when all the factories are loaded.
   *
   * Optimize a bit the containers in which our factories are stored.
   */
  virtual void optimize() noexcept = 0;
};

SCORE_LIB_BASE_EXPORT void
debug_types(const score::InterfaceBase* orig, const score::InterfaceBase* repl) noexcept;

class SCORE_LIB_BASE_EXPORT InterfaceListMain : public score::InterfaceListBase
{
public:
  InterfaceListMain();
  ~InterfaceListMain();
  void insert_base(std::unique_ptr<score::InterfaceBase> e, score::uuid_t k);

  mutable Nano::Signal<void(const score::InterfaceBase&)> added;

  auto reserve(std::size_t v) { return map.reserve(v); }
  auto size() const noexcept { return map.size(); }
  auto empty() const noexcept { return map.empty(); }

  void optimize() noexcept final override;
  ossia::hash_map<score::uuid_t, std::unique_ptr<score::InterfaceBase>> map;
};

/**
 * @brief InterfaceList Default implementation of InterfaceListBase
 *
 * The factories are stored in a hash_map. Keys shall be UUIDs.
 * This class can be used in range-based loops :
 *
 * \code
 * score::ApplicationContext& context = ...;
 * auto& iface_list = context.interfaces<MyInterfaceList>();
 * for(auto& iface : iface_list)
 * {
 *   auto res = iface.do_something(...);
 * }
 * \endcode
 *
 * An implementation of an interface shall never be inserted twice.
 */
template <typename FactoryType>
class InterfaceList : public score::InterfaceListMain
{
public:
  using factory_type = FactoryType;
  using key_type = typename FactoryType::ConcreteKey;
  using vector_type = IndirectContainer<FactoryType>;
  InterfaceList() = default;
  ~InterfaceList() = default;

  static const constexpr score::InterfaceKey static_interfaceKey() noexcept
  {
    return FactoryType::static_interfaceKey();
  }

  constexpr score::InterfaceKey interfaceKey() const noexcept final override
  {
    return FactoryType::static_interfaceKey();
  }

  void insert(std::unique_ptr<score::InterfaceBase> e) final override
  {
    if(auto result = dynamic_cast<factory_type*>(e.get()))
    {
      return insert_base(std::move(e), result->concreteKey().impl());
    }
    else
    {
      SCORE_SOFT_ASSERT("Invalid interface detected");
    }
  }

  //! Get a particular factory from its ConcreteKey
  FactoryType* get(const key_type& k) const noexcept
  {
    auto it = this->map.find(k.impl());
    return (it != this->map.end()) ? static_cast<FactoryType*>(it->second.get())
                                   : nullptr;
  }

  auto begin() noexcept
  {
    return make_indirect_cast_map_iterator<factory_type>(map.begin());
  }
  auto end() noexcept
  {
    return make_indirect_cast_map_iterator<factory_type>(map.end());
  }
  auto begin() const noexcept
  {
    return make_indirect_cast_map_iterator<factory_type>(map.begin());
  }
  auto end() const noexcept
  {
    return make_indirect_cast_map_iterator<factory_type>(map.end());
  }

  auto cbegin() const noexcept
  {
    return make_indirect_cast_map_iterator<factory_type>(map.cbegin());
  }
  auto cend() const noexcept
  {
    return make_indirect_cast_map_iterator<factory_type>(map.cend());
  }

private:
  InterfaceList(const InterfaceList&) = delete;
  InterfaceList(InterfaceList&&) = delete;
  InterfaceList& operator=(const InterfaceList&) = delete;
  InterfaceList& operator=(InterfaceList&&) = delete;
};

/**
 * @brief Utility class for making a factory interface list
 */
template <typename T>
class MatchingFactory : public score::InterfaceList<T>
{
public:
  /**
   * @brief Apply a function on the correct factory according to a set of
   * parameter.
   *
   * The factory must have a function `match` that takes some arguments, and
   * return `true` if these arguments are correct for the given factory.
   *
   * Then, the function passed in first argument is called on the actual
   * factory if it is found, else a default-constructed return value (so for
   * instance a null pointer).
   */
  template <typename Fun, typename... Args>
    requires std::is_invocable_v<
        Fun, typename score::InterfaceList<T>::factory_type&, Args...>
  auto make(Fun f, Args&&... args) const noexcept
  {
    using val_t = decltype(*this->begin());
    for(const val_t& elt : *this)
    {
      if(elt.matches(std::forward<Args>(args)...))
      {
        return (elt.*f)(std::forward<Args>(args)...);
      }
    }
    return decltype((std::declval<val_t>().*f)(std::forward<Args>(args)...)){};
  }
};
}
#endif
