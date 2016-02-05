#pragma once
#include <Explorer/Listening/ListeningHandler.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore_plugin_deviceexplorer_export.h>
namespace iscore
{
struct DocumentContext;
}
namespace Explorer
{
class DeviceDocumentPlugin;
using ListeningHandlerFactoryKey = UuidKey<ListeningHandler>;
class ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT ListeningHandlerFactory :
        public iscore::GenericFactoryInterface<ListeningHandlerFactoryKey>
{
        ISCORE_ABSTRACT_FACTORY_DECL(
                Explorer::ListeningHandler,
                "42828393-b8de-45a6-b79f-811eea2e1a40")

    public:
        virtual ~ListeningHandlerFactory();

        virtual std::unique_ptr<Explorer::ListeningHandler> make(
                const DeviceDocumentPlugin& plug,
                const iscore::DocumentContext& ctx) = 0;
};
}

Q_DECLARE_METATYPE(Explorer::ListeningHandlerFactoryKey)
