// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DefaultListeningHandlerFactory.hpp"

#include "DefaultListeningHandler.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/document/DocumentContext.hpp>
namespace Explorer
{
std::unique_ptr<Explorer::ListeningHandler> DefaultListeningHandlerFactory::make(
    const DeviceDocumentPlugin& plug,
    const score::DocumentContext& ctx)
{
  return std::make_unique<Explorer::DefaultListeningHandler>();
}
}
