#include "DefaultListeningHandlerFactory.hpp"
#include "DefaultListeningHandler.hpp"
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <iscore/document/DocumentContext.hpp>
namespace Explorer
{
std::unique_ptr<Explorer::ListeningHandler> DefaultListeningHandlerFactory::make(
        const DeviceDocumentPlugin& plug,
        const iscore::DocumentContext &ctx)
{
    return std::make_unique<Explorer::DefaultListeningHandler>();
}

}
