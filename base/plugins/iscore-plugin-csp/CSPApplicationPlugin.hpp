#pragma once
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>

class CSPApplicationPlugin : public iscore::GUIApplicationContextPlugin
{
    public:
        CSPApplicationPlugin(iscore::Presenter* pres);
        ~CSPApplicationPlugin() = default;


        iscore::DocumentDelegatePluginModel* loadDocumentPlugin(
                const QString& name,
                const VisitorVariant& var,
                iscore::DocumentModel* parent) override;


        void on_newDocument(iscore::Document* doc) override;
        void on_loadedDocument(iscore::Document* doc) override;

    protected:
        //void on_documentChanged() override;

    private:
};
