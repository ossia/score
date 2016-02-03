#pragma once

#include <Explorer/Listening/ListeningHandlerFactory.hpp>

namespace Explorer
{
class DefaultListeningHandlerFactory final :
        public ListeningHandlerFactory
{
    public:
        const ListeningHandlerFactoryKey&
        concreteFactoryKey() const override;

        ListeningHandler* make(
                const iscore::DocumentContext& ctx) override;
};
}
