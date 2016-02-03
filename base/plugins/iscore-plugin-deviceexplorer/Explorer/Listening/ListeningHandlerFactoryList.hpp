#pragma once
#include <Explorer/Listening/ListeningHandlerFactory.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore_plugin_deviceexplorer_export.h>
namespace Explorer
{
class ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT ListeningHandlerFactoryList final :
        public iscore::FactoryListInterface
{
        ISCORE_FACTORY_LIST_DECL(Explorer::ListeningHandlerFactory)

        public:
            virtual ~ListeningHandlerFactoryList();

        ListeningHandler* make(
                const iscore::DocumentContext& ctx);
};
}
