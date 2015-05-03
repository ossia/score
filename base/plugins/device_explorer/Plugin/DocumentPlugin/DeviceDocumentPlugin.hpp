#pragma once
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/command/OngoingCommandManager.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <DeviceExplorer/Protocol/DeviceList.hpp>
namespace iscore
{
    class Document;
}


class DeviceDocumentPlugin : public iscore::DocumentDelegatePluginModel
{
    public:
        DeviceDocumentPlugin(QObject* parent);
        DeviceDocumentPlugin(const VisitorVariant& loader,
                             QObject* parent);

        void serialize(const VisitorVariant&) const override;

    private:
        DeviceList m_list;
};

