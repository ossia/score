#pragma once
#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <type_traits>

/**
 * @brief Create a vector filled with pointers to new instances of the template arguments
 */
template <typename Base_T, typename... Args>
struct GenericFactoryInserter
{
  std::vector<std::unique_ptr<Base_T>> vec;
  GenericFactoryInserter() noexcept
  {
    vec.reserve(sizeof...(Args));
    for_each_type<TypeList<Args...>>(*this);
  }

  template <typename TheClass>
  void perform() noexcept
  {
    vec.push_back(std::make_unique<TheClass>());
  }
};

template <typename... Args>
auto make_ptr_vector() noexcept
{
  return GenericFactoryInserter<Args...>{}.vec;
}

/**
 * @class FactoryBuilder
 *
 * This class allows the user to customize the
 * creation of the factory by specializing it with the actual
 * factory type. An example is in score_plugin_scenario.cpp.
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

/**
 * @brief Fills an existing vector with factories instantiations
 */
template <typename Context_T, typename Base_T, typename... Args>
struct ContextualGenericFactoryFiller
{
  const Context_T& context;
  std::vector<std::unique_ptr<Base_T>>& vec;
  ContextualGenericFactoryFiller(const Context_T& ctx, std::vector<std::unique_ptr<Base_T>>& v) noexcept :
    context{ctx},
    vec{v}
  {
    vec.reserve(sizeof...(Args));
    for_each_type<TypeList<Args...>>(*this);
  }

  template <typename TheClass>
  void perform() noexcept
  {
    vec.push_back(FactoryBuilder<Context_T, TheClass>::make(context));
  }
};

template <typename Context_T, typename Base_T, typename... Args>
void fill_ptr_vector(const Context_T& context, std::vector<std::unique_ptr<Base_T>>& vec) noexcept
{
  ContextualGenericFactoryFiller<Context_T, Base_T, Args...>{context, vec};
}

/**
 * \class FW_T
 * \brief Used to group base classes and concrete classes in a single argument list.
 */
template <typename Factory_T, typename... Types_T>
struct FW_T
{
  static constexpr const auto size = sizeof...(Types_T);

  template <typename Base>
  static constexpr bool assert_baseof() noexcept
  {
    return true;
  }
  template <typename Base, typename Child, typename... Children>
  static constexpr bool assert_baseof() noexcept
  {
    return std::is_base_of<Base, Child>::value
           && assert_baseof<Base, Children...>();
  }

  template <typename Matcher_T>
  static bool visit(Matcher_T& matcher) noexcept
  {
    static_assert(
        assert_baseof<Factory_T, Types_T...>(),
        "A type is not child of the parent.");
    if (matcher.fact == Factory_T::static_interfaceKey())
    {
      fill_ptr_vector<std::remove_const_t<std::remove_reference_t<decltype(matcher.ctx)>>,
                      score::InterfaceBase,
                      Types_T...>(matcher.ctx, matcher.vec);
      return true;
    }

    return false;
  }
};

template <typename Factory_T, typename... Args>
using FW = FW_T<Factory_T, Args...>;

template <typename... Args>
struct TL : public TypeList<Args...>
{
};

namespace score
{
struct ApplicationContext;
}

template<typename Context_T>
struct FactoryMatcher
{
  const Context_T& ctx;
  const score::InterfaceKey& fact;
  std::vector<std::unique_ptr<score::InterfaceBase>>& vec;

  template <typename FactoryList_T>
  bool visit_if() const noexcept
  {
    return FactoryList_T::visit(*this);
  }
};


/**
 * @brief instantiate_factories Given a type list of factories, instantiate the ones corresponding to the key
 *
 * e.g.
 * \code
 * instantiate_factories<
 *  Context,
 *  TL<
 *    FW<AbstractType1, ConcreteType1_1, ConcreteType1_2>,
 *    FW<AbstractType2, ConcreteType2_1>
 *  >>(context, KeyOfAbstractType1, vec);
 * \endcode
 *
 * would return a vector such as :
 * \code
 * { std::make_unique<ConcreteType1_1>(), std::make_unique<ConcreteType1_2>() }
 * \endcode
 */
template <typename Context_T, typename... Args>
auto instantiate_factories(
    const Context_T& ctx, const score::InterfaceKey& key) noexcept
{
  std::vector<std::unique_ptr<score::InterfaceBase>> vec;

  for_each_type_if<TL<Args...>>(FactoryMatcher<Context_T>{ctx, key, vec});

  return vec;
}
