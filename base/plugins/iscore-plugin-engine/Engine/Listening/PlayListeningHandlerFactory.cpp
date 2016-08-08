#include "PlayListeningHandlerFactory.hpp"
#include "PlayListeningHandler.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>

#include <iscore/document/DocumentContext.hpp>
namespace Engine
{
namespace Execution
{
std::unique_ptr<Explorer::ListeningHandler> PlayListeningHandlerFactory::make(
        const Explorer::DeviceDocumentPlugin& plug,
        const iscore::DocumentContext &ctx)
{
    auto& exe = ctx.plugin<Engine::Execution::DocumentPlugin>();
    return std::make_unique<PlayListeningHandler>(exe);
}
}
}
