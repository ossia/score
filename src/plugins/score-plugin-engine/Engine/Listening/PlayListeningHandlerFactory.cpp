// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PlayListeningHandlerFactory.hpp"

#include "PlayListeningHandler.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Execution/DocumentPlugin.hpp>

#include <score/document/DocumentContext.hpp>
namespace Execution
{
std::unique_ptr<Explorer::ListeningHandler> PlayListeningHandlerFactory::make(
    const Explorer::DeviceDocumentPlugin& plug, const score::DocumentContext& ctx)
{
  auto& exe = ctx.plugin<Execution::DocumentPlugin>();
  return std::make_unique<PlayListeningHandler>(exe);
}
}
