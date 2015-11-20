#pragma once
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>

class DeviceExplorerControl final : public iscore::GUIApplicationContextPlugin
{
    public:
        DeviceExplorerControl(iscore::Application& app);

        virtual iscore::DocumentDelegatePluginModel* loadDocumentPlugin(
                const QString& name,
                const VisitorVariant& var,
                iscore::Document *parent) override;

    protected:
        void on_newDocument(iscore::Document* doc) override;
        void on_documentChanged(
                iscore::Document* olddoc,
                iscore::Document* newdoc) override;
};
