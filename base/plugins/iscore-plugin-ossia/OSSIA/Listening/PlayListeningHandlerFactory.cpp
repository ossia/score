#include "PlayListeningHandlerFactory.hpp"
#include "PlayListeningHandler.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>

#include <iscore/document/DocumentContext.hpp>
namespace Ossia
{
std::unique_ptr<Explorer::ListeningHandler> PlayListeningHandlerFactory::make(
        const Explorer::DeviceDocumentPlugin& plug,
        const iscore::DocumentContext &ctx)
{
    auto& exe = ctx.plugin<RecreateOnPlay::DocumentPlugin>();
    return std::make_unique<PlayListeningHandler>(plug.list(), exe);
}
}
