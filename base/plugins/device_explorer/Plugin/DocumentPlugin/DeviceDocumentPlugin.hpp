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
        Q_OBJECT
    public:
        DeviceDocumentPlugin(QObject* parent);
        DeviceDocumentPlugin(const VisitorVariant& loader,
                             QObject* parent);

        void serialize(const VisitorVariant&) const override;

    signals:
        // These go to the device explorer model.
        void pathAdded(const AddressSettings&);
        void pathUpdated(const AddressSettings&);
        void pathRemoved(const QString&);

    private:
        DeviceList m_list;
};

