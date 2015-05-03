#pragma once
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/command/OngoingCommandManager.hpp>
#include <iscore/serialization/VisitorCommon.hpp>

namespace iscore
{
    class Document;
}


class DeviceDocumentPlugin : public iscore::DocumentDelegatePluginModel
{
    public:
        DeviceDocumentPlugin(iscore::DocumentModel *doc);

        //NetworkDocumentPlugin(const VisitorVariant& loader, iscore::DocumentModel* doc);

        void serialize(const VisitorVariant&) const override { }
};

