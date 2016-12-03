#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>

namespace iscore
{

template <typename... Types>
struct MakeArgs
{
};
template <typename... Types>
struct LoadArgs
{
};
template <typename Model_T, typename MakeTuple, typename LoadTuple>
class GenericModelFactory;

template <typename Model_T, typename... MakeArgs_T, typename... LoadArgs_T>
class
    GenericModelFactory<Model_T, MakeArgs<MakeArgs_T...>, LoadArgs<LoadArgs_T...>>
{
public:
  virtual ~GenericModelFactory() = default;
  virtual QString prettyName() const = 0;
  virtual Model_T* make(MakeArgs_T...) = 0;
  virtual Model_T* load(LoadArgs_T...) = 0;
};

// TODO try to find how to use me.
template <typename Model_T, typename MakeTuple>
class GenericComponentFactory_Make;

template <typename Model_T, typename... MakeArgs_T>
class GenericComponentFactory_Make<Model_T, MakeArgs<MakeArgs_T...>>
{
public:
  using Args = MakeArgs<MakeArgs_T...>;

  virtual ~GenericComponentFactory_Make() = default;
  virtual Model_T* make(MakeArgs_T...) const = 0;
};

template <typename Component_T, typename ComponentFactoryBase_T>
class GenericComponentFactoryImpl : public ComponentFactoryBase_T
{
  using model_type = typename Component_T::model_type;
  using base_model_type = typename ComponentFactoryBase_T::base_model_type;
  using system_type = typename ComponentFactoryBase_T::system_type;
  using component_type = Component_T;
  using ConcreteFactoryKey =
      typename ComponentFactoryBase_T::ConcreteFactoryKey;
  using ComponentFactoryBase_T::ComponentFactoryBase_T;

  static auto static_concreteFactoryKey()
  {
    return Component_T::static_key().impl();
  }

  ConcreteFactoryKey concreteFactoryKey() const final override
  {
    return Component_T::static_key().impl(); // Note : here there is a
                                             // conversion between
                                             // UuidKey<Component> and
                                             // ConcreteFactoryKey
  }

  bool matches(const base_model_type& p) const final override
  {
    return dynamic_cast<const model_type*>(&p);
  }
};
}
