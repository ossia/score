#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>

namespace iscore
{

template <typename... Types>
struct MakeArgs {};
template <typename... Types>
struct LoadArgs {};
template <typename Model_T, typename MakeTuple, typename LoadTuple> class GenericModelFactory;

template <
        typename Model_T,
        typename... MakeArgs_T,
        typename... LoadArgs_T>
class GenericModelFactory<Model_T, MakeArgs<MakeArgs_T...>, LoadArgs<LoadArgs_T...>>
{
    public:
        virtual ~GenericModelFactory() = default;
        virtual QString prettyName() const = 0;
        virtual Model_T* make(MakeArgs_T...) = 0;
        virtual Model_T* load(LoadArgs_T...) = 0;
};

}
