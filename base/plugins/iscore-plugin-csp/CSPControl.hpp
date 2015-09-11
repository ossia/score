#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>

class CSPControl : public iscore::PluginControlInterface
{
    public:
        CSPControl(iscore::Presenter* pres);
        ~CSPControl() = default;


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
