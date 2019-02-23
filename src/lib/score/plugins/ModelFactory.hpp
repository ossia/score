#pragma once
#include <score/plugins/Interface.hpp>

namespace score
{
template <typename Component_T, typename ComponentFactoryBase_T>
class GenericComponentFactoryImpl : public ComponentFactoryBase_T
{
  using model_type = typename Component_T::model_type;
  using base_model_type = typename ComponentFactoryBase_T::base_model_type;
  using system_type = typename ComponentFactoryBase_T::system_type;
  using component_type = Component_T;
  using ConcreteKey = typename ComponentFactoryBase_T::ConcreteKey;
  using ComponentFactoryBase_T::ComponentFactoryBase_T;

  static auto static_concreteKey() { return Component_T::static_key().impl(); }

  ConcreteKey concreteKey() const noexcept final override
  {
    return Component_T::static_key().impl(); // Note : here there is a
                                             // conversion between
                                             // UuidKey<Component> and
                                             // ConcreteKey
  }

  bool matches(const base_model_type& p) const final override
  {
    return dynamic_cast<const model_type*>(&p);
  }
};
}
