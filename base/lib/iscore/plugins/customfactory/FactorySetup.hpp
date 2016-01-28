#pragma once
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <type_traits>

template<typename Factory_T, typename... Types_T>
struct FW_T
{
        template<typename Base>
        static constexpr bool assert_baseof()
        {
            return true;
        }
        template<typename Base, typename Child, typename... Children>
        static constexpr bool assert_baseof()
        {
            return std::is_base_of<Base, Child>::value
                 && assert_baseof<Base, Children...>();
        }

        template<typename Matcher_T>
        static bool visit(Matcher_T& matcher)
        {
            static_assert(assert_baseof<Factory_T, Types_T...>(), "A type is not child of the parent.");
            if(matcher.fact == Factory_T::static_abstractFactoryKey())
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
template<typename Factory_T, typename ... Args>
using FW = FW_T<Factory_T, Args...>;
template<typename ... Args>
using TL = TypeList<Args...>;

namespace iscore
{
struct ApplicationContext;
}

struct FactoryMatcher
{
        const iscore::ApplicationContext& ctx;
        const iscore::AbstractFactoryKey& fact;
        std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>>& vec;

        template<typename FactoryList_T> bool visit_if() const
        {
            return FactoryList_T::visit(*this);
        }
};

template<typename Context_T, typename Factories_T>
auto instantiate_factories(
        const Context_T& ctx,
        const iscore::AbstractFactoryKey& key)
{
    std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> vec;

    for_each_type_if<Factories_T>(FactoryMatcher{ctx, key, vec});

    return vec;
}
