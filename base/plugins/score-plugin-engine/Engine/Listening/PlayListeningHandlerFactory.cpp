// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PlayListeningHandlerFactory.hpp"

#include "PlayListeningHandler.hpp"

#include <Engine/Executor/DocumentPlugin.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <score/document/DocumentContext.hpp>
namespace Engine
{
namespace Execution
{
std::unique_ptr<Explorer::ListeningHandler> PlayListeningHandlerFactory::make(
    const Explorer::DeviceDocumentPlugin& plug,
    const score::DocumentContext& ctx)
{
  auto& exe = ctx.plugin<Engine::Execution::DocumentPlugin>();
  return std::make_unique<PlayListeningHandler>(exe);
}
}
}
