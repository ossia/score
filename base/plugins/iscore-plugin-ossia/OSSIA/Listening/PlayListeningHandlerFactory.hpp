#pragma once

#include <Explorer/Listening/ListeningHandlerFactory.hpp>

namespace Ossia
{
class PlayListeningHandlerFactory final :
        public Explorer::ListeningHandlerFactory
{
    public:
        const Explorer::ListeningHandlerFactoryKey&
        concreteFactoryKey() const override;

        Explorer::ListeningHandler* make(
                const iscore::DocumentContext& ctx) override;
};
}
