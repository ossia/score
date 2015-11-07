#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>

class DeviceExplorerControl final : public iscore::PluginControlInterface
{
    public:
        DeviceExplorerControl(iscore::Presenter*);

        virtual iscore::DocumentDelegatePluginModel* loadDocumentPlugin(
                const QString& name,
                const VisitorVariant& var,
                iscore::DocumentModel *parent) override;

    protected:
        void on_newDocument(iscore::Document* doc) override;
        void on_documentChanged(
                iscore::Document* olddoc,
                iscore::Document* newdoc) override;
};
