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
    return deserialize_dyn(var, [&] (auto&& deserializer)
    { return new DeviceDocumentPlugin{doc, deserializer, parent}; });
}
}
