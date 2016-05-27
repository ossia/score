#pragma once
#include <Explorer/Listening/ListeningHandlerFactory.hpp>

#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore_plugin_deviceexplorer_export.h>
namespace Explorer
{
class ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT ListeningHandlerFactoryList final :
        public iscore::ConcreteFactoryList<Explorer::ListeningHandlerFactory>
{
        public:
            virtual ~ListeningHandlerFactoryList();

        std::unique_ptr<Explorer::ListeningHandler> make(
                const Explorer::DeviceDocumentPlugin& plug,
                const iscore::DocumentContext& ctx) const;
};
}
