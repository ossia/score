#include "DefaultListeningHandlerFactory.hpp"
#include "DefaultListeningHandler.hpp"
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <iscore/document/DocumentContext.hpp>
namespace Explorer
{

const ListeningHandlerFactoryKey &
DefaultListeningHandlerFactory::concreteFactoryKey() const
{
    static const ListeningHandlerFactoryKey k{"2e7498f9-ac41-4844-8f73-65e09669b582"};
    return k;
}

ListeningHandler* DefaultListeningHandlerFactory::make(
        const iscore::DocumentContext &ctx)
{
    auto& doc = ctx.plugin<Explorer::DeviceDocumentPlugin>();
    return new DefaultListeningHandler{doc.list()};
}

}
