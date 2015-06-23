#pragma once
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>

#include <iscore/serialization/VisitorCommon.hpp>
#include <DeviceExplorer/Protocol/DeviceList.hpp>
namespace iscore
{
    class Document;
}

// The DeviceDocumentPlugin is informed of new addresses by the plugin-provided models of the
// devices, and propagates them.
// Exploration should be a Command, which registers all the new addresses in its constructor
// - via a thread and updateNamespace()-
// and add them correctly in the explorer model.
class DeviceDocumentPlugin : public iscore::DocumentDelegatePluginModel
{
        Q_OBJECT
    public:
        DeviceDocumentPlugin(QObject* parent);
        DeviceDocumentPlugin(const VisitorVariant& loader,
                             QObject* parent);

        void serialize(const VisitorVariant&) const override;

        DeviceList& list()
        { return m_list; }

        const DeviceList& list() const
        { return m_list; }

    private:
        DeviceList m_list;
};

