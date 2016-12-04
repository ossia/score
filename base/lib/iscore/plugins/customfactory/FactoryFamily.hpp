#pragma once
#include <ossia/detail/algorithms.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/tools/ForEachType.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore/tools/std/IndirectContainer.hpp>
#include <iscore/tools/std/Pointer.hpp>

#include <iscore/tools/std/HashMap.hpp>

#include <QMetaType>
#include <iscore_lib_base_export.h>

namespace iscore
{
/**
 * @brief Interface to access factories.
 *
 * Actual instances are available through iscore::ApplicationContext:
 *
 * @code
 * auto& factories = context.components.factory<MyConcreteFactoryList>();
 * @endcode
 */
class ISCORE_LIB_BASE_EXPORT FactoryListInterface
{
public:
  static constexpr bool factory_list_tag = true;
  FactoryListInterface() noexcept { }
  FactoryListInterface(const FactoryListInterface&) = delete;
  FactoryListInterface& operator=(const FactoryListInterface&) = delete;
  virtual ~FactoryListInterface();

  //! A key that uniquely identifies this family of factories.
  virtual iscore::AbstractFactoryKey abstractFactoryKey() const noexcept = 0;

  /**
   * @brief insert Register a new factory.
   *
   * All the factories are registered upon loading.
   */
  virtual void insert(std::unique_ptr<iscore::FactoryInterfaceBase>) = 0;

  /**
   * @brief optimize Called when all the factories are loaded.
   *
   * Optimize a bit the containers in which our factories are stored.
   */
  virtual void optimize() noexcept = 0;
};

template <typename FactoryType>
/**
 * @brief Default implementation of FactoryListInterface
 *
 * The factories are stored in a hash_map. Keys should be UUIDs.
 */
class ConcreteFactoryList : public iscore::FactoryListInterface,
                            public IndirectUnorderedMap<iscore::hash_map<
                                typename FactoryType::ConcreteFactoryKey,
                                std::unique_ptr<FactoryType>>>
{
public:
  using factory_type = FactoryType;
  using key_type = typename FactoryType::ConcreteFactoryKey;
  ConcreteFactoryList() noexcept
  {
  }

  ~ConcreteFactoryList() noexcept override = default;

  static const constexpr iscore::AbstractFactoryKey static_abstractFactoryKey() noexcept
  {
    return FactoryType::static_abstractFactoryKey();
  }

  iscore::AbstractFactoryKey abstractFactoryKey() const noexcept final override
  {
    return FactoryType::static_abstractFactoryKey();
  }

  void insert(std::unique_ptr<iscore::FactoryInterfaceBase> e) final override
  {
    if (auto result = dynamic_cast<factory_type*>(e.get()))
    {
      e.release();
      std::unique_ptr<factory_type> pf{result};

      auto k = pf->concreteFactoryKey();
      auto it = this->map.find(k);
      ISCORE_ASSERT(it == this->map.end());

      this->map.emplace(std::make_pair(k, std::move(pf)));
    }
  }

  FactoryType* get(const key_type& k) const noexcept
  {
    auto it = this->map.find(k);
    return (it != this->map.end()) ? it->second.get() : nullptr;
  }

  const auto& ptr_list() const noexcept
  {
    return this->map;
  }

private:
  void optimize() noexcept final override
  {
    iscore::optimize_hash_map(this->map);
  }

  ConcreteFactoryList(const ConcreteFactoryList&) = delete;
  ConcreteFactoryList(ConcreteFactoryList&&) = delete;
  ConcreteFactoryList& operator=(const ConcreteFactoryList&) = delete;
  ConcreteFactoryList& operator=(ConcreteFactoryList&&) = delete;
};

template <typename T>
class MatchingFactory : public iscore::ConcreteFactoryList<T>
{
public:
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
