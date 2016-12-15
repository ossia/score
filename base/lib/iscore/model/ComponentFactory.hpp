#pragma once
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>

#define ISCORE_CONCRETE_COMPONENT_FACTORY(AbstractFactory, ConcreteFactory)

#define ISCORE_ABSTRACT_COMPONENT_FACTORY(Type)                        \
public:                                                                \
  static Q_DECL_RELAXED_CONSTEXPR iscore::InterfaceKey           \
  static_interfaceKey() noexcept                                          \
  {                                                                    \
    return static_cast<iscore::InterfaceKey>(                    \
        Type::static_key().impl());                                    \
  }                                                                    \
                                                                       \
  iscore::InterfaceKey interfaceKey() const noexcept final override \
  {                                                                    \
    return static_interfaceKey();                                \
  }                                                                    \
                                                                       \
private:

namespace iscore
{
struct DocumentContext;
template <
    typename Model_T,            // e.g. ProcessModel - maybe ProcessEntity ?
    typename System_T,           // e.g. LocalTree::DocumentPlugin
    typename ComponentFactory_T> // e.g. ProcessComponent
class GenericComponentFactory
    : public iscore::Interface<ComponentFactory_T>
{
public:
  using base_model_type = Model_T;
  using system_type = System_T;
  using factory_type = ComponentFactory_T;

  virtual bool matches(const base_model_type&) const = 0;
};

template <
    typename Model_T,   // e.g. ProcessModel - maybe ProcessEntity ?
    typename System_T,  // e.g. LocalTree::DocumentPlugin
    typename Factory_T> // e.g. ProcessComponentFactory
class GenericComponentFactoryList final
    : public iscore::InterfaceList<Factory_T>
{
public:
  template <typename... Args>
  Factory_T* factory(Args&&... args) const
  {
    for (auto& factory : *this)
    {
      if (factory.matches(std::forward<Args>(args)...))
      {
        return &factory;
      }
    }

    return nullptr;
  }
};

template <
    typename Model_T,   // e.g. ProcessModel - maybe ProcessEntity ?
    typename System_T,  // e.g. LocalTree::DocumentPlugin
    typename Factory_T, // e.g. ProcessComponentFactory
    typename DefaultFactory_T>
class DefaultedGenericComponentFactoryList final
    : public iscore::InterfaceList<Factory_T>
{
public:
  template <typename... Args>
  Factory_T& factory(Args&&... args) const
  {
    for (auto& factory : *this)
    {
      if (factory.matches(std::forward<Args>(args)...))
      {
        return factory;
      }
    }

    return m_default;
  }

private:
  mutable DefaultFactory_T m_default;
};
}
