#pragma once
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <QString>

#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>

namespace iscore {

class Document;
}  // namespace iscore
struct VisitorVariant;

namespace DeviceExplorer
{
class DeviceExplorerApplicationPlugin final : public iscore::GUIApplicationContextPlugin
{
    public:
        DeviceExplorerApplicationPlugin(const iscore::ApplicationContext& app);

        virtual iscore::DocumentPluginModel* loadDocumentPlugin(
                const QString& name,
                const VisitorVariant& var,
                iscore::Document *parent) override;

    protected:
        void on_newDocument(iscore::Document* doc) override;
        void on_documentChanged(
                iscore::Document* olddoc,
                iscore::Document* newdoc) override;
};
}
