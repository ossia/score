#include "PlayListeningHandlerFactory.hpp"
#include "PlayListeningHandler.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>

#include <iscore/document/DocumentContext.hpp>
namespace Ossia
{

const Explorer::ListeningHandlerFactoryKey &
PlayListeningHandlerFactory::concreteFactoryKey() const
{
    static const Explorer::ListeningHandlerFactoryKey k{
        "5332e60c-2e29-490a-a12e-c289c5262c57"};
    return k;
}

Explorer::ListeningHandler* PlayListeningHandlerFactory::make(
        const iscore::DocumentContext &ctx)
{
    auto& doc = ctx.plugin<Explorer::DeviceDocumentPlugin>();
    auto& exe = ctx.plugin<RecreateOnPlay::DocumentPlugin>();
    return new PlayListeningHandler{doc.list(), exe};
}
}
