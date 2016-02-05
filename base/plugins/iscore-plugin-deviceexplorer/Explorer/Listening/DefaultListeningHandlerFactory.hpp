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

        std::unique_ptr<Explorer::ListeningHandler> make(
                const DeviceDocumentPlugin& plug,
                const iscore::DocumentContext& ctx) override;
};
}
