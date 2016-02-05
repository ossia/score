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

        std::unique_ptr<Explorer::ListeningHandler> make(
                const Explorer::DeviceDocumentPlugin& plug,
                const iscore::DocumentContext& ctx) override;
};
}
