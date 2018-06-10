#pragma once
#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <score/plugins/customfactory/FactoryInterface.hpp>

#define SCORE_CONCRETE_COMPONENT_FACTORY(AbstractFactory, ConcreteFactory)

#define SCORE_ABSTRACT_COMPONENT_FACTORY(Type)                          \
public:                                                                 \
  static Q_DECL_RELAXED_CONSTEXPR score::InterfaceKey                   \
  static_interfaceKey() noexcept                                        \
  {                                                                     \
    return static_cast<score::InterfaceKey>(Type::static_key().impl()); \
  }                                                                     \
                                                                        \
  score::InterfaceKey interfaceKey() const noexcept final override      \
  {                                                                     \
    return static_interfaceKey();                                       \
  }                                                                     \
                                                                        \
private:

namespace score
{
struct DocumentContext;
template <
    typename Model_T,            // e.g. ProcessModel - maybe ProcessEntity ?
    typename System_T,           // e.g. LocalTree::DocumentPlugin
    typename ComponentFactory_T> // e.g. ProcessComponent
class GenericComponentFactory : public score::InterfaceBase
{
public:
  using base_model_type = Model_T;
  using system_type = System_T;
  using factory_type = ComponentFactory_T;

  using ConcreteKey = UuidKey<ComponentFactory_T>;

  //! Identifies an implementation of an interface uniquely
  virtual ConcreteKey concreteKey() const noexcept = 0;

  virtual bool matches(const base_model_type&) const = 0;
};

template <
    typename Model_T,   // e.g. ProcessModel - maybe ProcessEntity ?
    typename System_T,  // e.g. LocalTree::DocumentPlugin
    typename Factory_T> // e.g. ProcessComponentFactory
class GenericComponentFactoryList : public score::InterfaceList<Factory_T>
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
    : public score::InterfaceList<Factory_T>
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
