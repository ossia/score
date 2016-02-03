#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore_plugin_deviceexplorer_export.h>
namespace iscore
{
struct DocumentContext;
}
namespace Explorer
{
class ListeningHandler;
using ListeningHandlerFactoryKey = UuidKey<ListeningHandler>;
class ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT ListeningHandlerFactory :
        public iscore::GenericFactoryInterface<ListeningHandlerFactoryKey>
{
        ISCORE_ABSTRACT_FACTORY_DECL(
                Explorer::ListeningHandler,
                "42828393-b8de-45a6-b79f-811eea2e1a40")

    public:
        virtual ~ListeningHandlerFactory();

        virtual ListeningHandler* make(const iscore::DocumentContext& ctx) = 0;
};
}

Q_DECLARE_METATYPE(Explorer::ListeningHandlerFactoryKey)
