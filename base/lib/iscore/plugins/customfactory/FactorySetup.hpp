#pragma once
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <type_traits>


template <typename Base_T, typename... Args>
struct GenericFactoryInserter
{
  std::vector<std::unique_ptr<Base_T>> vec;
  GenericFactoryInserter()
  {
    vec.reserve(sizeof...(Args));
    for_each_type<TypeList<Args...>>(*this);
  }

  template <typename TheClass>
  void perform()
  {
    vec.push_back(std::make_unique<TheClass>());
  }
};

template <typename... Args>
auto make_ptr_vector()
{
  return GenericFactoryInserter<Args...>{}.vec;
}

/**
 * @brief FactoryBuilder
 *
 * This class allows the user to customize the
 * creation of the factory by specializing it with the actual
 * factory type. An example is in iscore_plugin_scenario.cpp.
 */
template <
    typename Context_T,
    typename Factory_T>
struct FactoryBuilder // sorry padre for I have sinned
{
  static auto make(const Context_T&)
  {
    return std::make_unique<Factory_T>();
  }
};

template <typename Context_T, typename Base_T, typename... Args>
struct ContextualGenericFactoryInserter
{
  const Context_T& context;
  std::vector<std::unique_ptr<Base_T>> vec;
  ContextualGenericFactoryInserter(const Context_T& ctx) : context{ctx}
  {
    vec.reserve(sizeof...(Args));
    for_each_type<TypeList<Args...>>(*this);
  }

  template <typename TheClass>
  void perform()
  {
    vec.push_back(FactoryBuilder<Context_T, TheClass>::make(context));
  }
};

template <typename Context_T, typename Base_T, typename... Args>
auto make_ptr_vector(const Context_T& context)
{
  return ContextualGenericFactoryInserter<Context_T, Base_T, Args...>{context}
      .vec;
}

template <typename Factory_T, typename... Types_T>
struct FW_T
{
  static constexpr const auto size = sizeof...(Types_T);

  template <typename Base>
  static constexpr bool assert_baseof()
  {
    return true;
  }
  template <typename Base, typename Child, typename... Children>
  static constexpr bool assert_baseof()
  {
    return std::is_base_of<Base, Child>::value
           && assert_baseof<Base, Children...>();
  }

  template <typename Matcher_T>
  static bool visit(Matcher_T& matcher)
  {
    static_assert(
        assert_baseof<Factory_T, Types_T...>(),
        "A type is not child of the parent.");
    if (matcher.fact == Factory_T::static_abstractFactoryKey())
    {
      matcher.vec = make_ptr_vector<
                        std::remove_const_t<std::remove_reference_t<decltype(matcher.ctx)>>,
                        iscore::FactoryInterfaceBase,
                        Types_T...>(matcher.ctx);
      return true;
    }

    return false;
  }
};

template <typename... Args>
struct counter
{
  static const constexpr auto size = 0;
};
template <typename Arg, typename... Args>
struct counter<Arg, Args...>
{
  static const constexpr auto size = Arg::size + counter<Args...>::size;
};

template <typename Factory_T, typename... Args>
using FW = FW_T<Factory_T, Args...>;

template <typename... Args>
struct TL : public TypeList<Args...>
{
public:
  // Returns number total number of concrete factories.
  static constexpr auto count()
  {
    return counter<Args...>::size;
  }
};

namespace iscore
{
struct ApplicationContext;
}

struct FactoryMatcher
{
  const iscore::ApplicationContext& ctx;
  const iscore::AbstractFactoryKey& fact;
  std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>>& vec;

  template <typename FactoryList_T>
  bool visit_if() const
  {
    return FactoryList_T::visit(*this);
  }
};

template <typename Context_T, typename Factories_T>
auto instantiate_factories(
    const Context_T& ctx, const iscore::AbstractFactoryKey& key)
{
  std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> vec;
  vec.reserve(Factories_T::count());

  for_each_type_if<Factories_T>(FactoryMatcher{ctx, key, vec});

  return vec;
}
