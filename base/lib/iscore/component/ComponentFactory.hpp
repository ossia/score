#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

#define ISCORE_COMPONENT_FACTORY( AbstractFactory, ConcreteFactory )

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
        virtual bool matches(
                Model_T&,
                const System_T&) const = 0;
};


template<
        typename Model_T, // e.g. ProcessModel - maybe ProcessEntity ?
        typename System_T, // e.g. LocalTree::DocumentPlugin
        typename Factory_T> // e.g. ProcessComponentFactory
class GenericComponentFactoryList final :
        public iscore::ConcreteFactoryList<Factory_T>
{
    public:
        Factory_T* factory(
                Model_T& model,
                const System_T& doc) const
        {
            for(auto& factory : *this)
            {
                if(factory.matches(model, doc))
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
        Factory_T& factory(
                Model_T& model,
                const System_T& doc) const
        {
            for(auto& factory : *this)
            {
                if(factory.matches(model, doc))
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
