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

std::unique_ptr<Explorer::ListeningHandler> PlayListeningHandlerFactory::make(
        const Explorer::DeviceDocumentPlugin& plug,
        const iscore::DocumentContext &ctx)
{
    auto& exe = ctx.plugin<RecreateOnPlay::DocumentPlugin>();
    return std::make_unique<PlayListeningHandler>(plug.list(), exe);
}
}
