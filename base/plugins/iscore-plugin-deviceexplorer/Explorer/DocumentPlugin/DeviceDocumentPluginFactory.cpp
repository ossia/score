#include "DeviceDocumentPluginFactory.hpp"
#include "DeviceDocumentPlugin.hpp"
#include <Explorer/Listening/ListeningHandlerFactoryList.hpp>
namespace Explorer
{
iscore::DocumentPlugin* DocumentPluginFactory::load(
        const VisitorVariant& var,
        iscore::DocumentContext& doc,
        QObject* parent)
{
    // TODO smell
    return new DeviceDocumentPlugin{
                doc,
                var,
                parent};
}
}
