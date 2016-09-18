#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

#define ISCORE_CONCRETE_COMPONENT_FACTORY( AbstractFactory, ConcreteFactory )


#define ISCORE_ABSTRACT_COMPONENT_FACTORY( Type ) \
    public: \
    static Q_DECL_RELAXED_CONSTEXPR iscore::AbstractFactoryKey static_abstractFactoryKey() { \
        return static_cast<iscore::AbstractFactoryKey>(Type::static_key().impl()); \
    } \
    \
    iscore::AbstractFactoryKey abstractFactoryKey() const final override { \
        return static_abstractFactoryKey(); \
    } \
    private:

namespace iscore
{
struct DocumentContext;
template<
        typename Model_T, // e.g. ProcessModel - maybe ProcessEntity ?
        typename System_T, // e.g. LocalTree::DocumentPlugin
        typename ComponentFactory_T> // e.g. ProcessComponent
class GenericComponentFactory :
        public iscore::AbstractFactory<ComponentFactory_T>
{
    public:
        using base_model_type = Model_T;
        using system_type = System_T;
        using factory_type = ComponentFactory_T;

        virtual bool matches(const base_model_type&) const = 0;
};


template<
        typename Model_T, // e.g. ProcessModel - maybe ProcessEntity ?
        typename System_T, // e.g. LocalTree::DocumentPlugin
        typename Factory_T> // e.g. ProcessComponentFactory
class GenericComponentFactoryList final :
        public iscore::ConcreteFactoryList<Factory_T>
{
    public:
        template<typename... Args>
        Factory_T* factory(Args&&... args) const
        {
            for(auto& factory : *this)
            {
                if(factory.matches(std::forward<Args>(args)...))
                {
                    return &factory;
                }
            }

            return nullptr;
        }
};

template<
        typename Model_T, // e.g. ProcessModel - maybe ProcessEntity ?
        typename System_T, // e.g. LocalTree::DocumentPlugin
        typename Factory_T, // e.g. ProcessComponentFactory
        typename DefaultFactory_T>
class DefaultedGenericComponentFactoryList final :
        public iscore::ConcreteFactoryList<Factory_T>
{
    public:
        template<typename... Args>
        Factory_T& factory(Args&&... args) const
        {
            for(auto& factory : *this)
            {
                if(factory.matches(std::forward<Args>(args)...))
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
