#ifndef FACTORY_FAMILY_HPP_2016_12_08_18_31
#define FACTORY_FAMILY_HPP_2016_12_08_18_31

#include <ossia/detail/algorithms.hpp>

#include <QMetaType>
#include <score/plugins/customfactory/FactoryInterface.hpp>
#include <score/tools/ForEachType.hpp>
#include <score/tools/Todo.hpp>
#include <score/tools/std/HashMap.hpp>
#include <score/tools/std/IndirectContainer.hpp>
#include <score/tools/std/Pointer.hpp>
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
 * \encode
 *
 * An implementation of an interface shall never be inserted twice.
 */
template <typename FactoryType>
class InterfaceList : public score::InterfaceListBase
{
public:
  using factory_type = FactoryType;
  using key_type = typename FactoryType::ConcreteKey;
  InterfaceList() = default;
  ~InterfaceList() = default;

  static const constexpr score::InterfaceKey static_interfaceKey() noexcept
  {
    return FactoryType::static_interfaceKey();
  }

  score::InterfaceKey interfaceKey() const noexcept final override
  {
    return FactoryType::static_interfaceKey();
  }

  void insert(std::unique_ptr<score::InterfaceBase> e) final override
  {
    if (auto result = dynamic_cast<factory_type*>(e.get()))
    {
      e.release();
      std::unique_ptr<factory_type> pf{result};

      auto k = pf->concreteKey();
      auto it = this->map.find(k);
      SCORE_ASSERT(it == this->map.end());

      this->map.emplace(std::make_pair(k, std::move(pf)));
    }
  }

  //! Get a particular factory from its ConcreteKey
  FactoryType* get(const key_type& k) const noexcept
  {
    auto it = this->map.find(k);
    return (it != this->map.end()) ? it->second.get() : nullptr;
  }

  auto begin() noexcept
  {
    return make_indirect_map_iterator(map.begin());
  }
  auto begin() const noexcept
  {
    return make_indirect_map_iterator(map.begin());
  }

  auto cbegin() noexcept
  {
    return make_indirect_map_iterator(map.cbegin());
  }
  auto cbegin() const noexcept
  {
    return make_indirect_map_iterator(map.cbegin());
  }

  auto end() noexcept
  {
    return make_indirect_map_iterator(map.end());
  }
  auto end() const noexcept
  {
    return make_indirect_map_iterator(map.end());
  }

  auto cend() noexcept
  {
    return make_indirect_map_iterator(map.cend());
  }
  auto cend() const noexcept
  {
    return make_indirect_map_iterator(map.cend());
  }

  auto empty() const noexcept
  {
    return map.empty();
  }

  template <typename K>
  auto find(K&& key) const noexcept
  {
    return make_indirect_map_iterator(map.find(std::forward<K>(key)));
  }

  auto size() const noexcept
  {
    return map.size();
  }

protected:
  score::
      hash_map<typename FactoryType::ConcreteKey, std::unique_ptr<FactoryType>>
          map;

private:
  void optimize() noexcept final override
  {
    score::optimize_hash_map(this->map);
  }

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
  auto make(Fun f, Args&&... args) const noexcept
  {
    using val_t = decltype(*this->begin());
    auto it = ossia::find_if(*this, [&](const val_t& elt) {
      return elt.matches(std::forward<Args>(args)...);
    });

    return (it != this->end())
               ? ((*it).*f)(std::forward<Args>(args)...)
               : decltype(((*it).*f)(std::forward<Args>(args)...)){};
  }
};
}
#endif
