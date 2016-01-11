#pragma once
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>

class CSPApplicationPlugin final : public iscore::GUIApplicationContextPlugin
{
    public:
        CSPApplicationPlugin(const iscore::ApplicationContext& pres);
        ~CSPApplicationPlugin() = default;


        iscore::DocumentPluginModel* loadDocumentPlugin(
                const QString& name,
                const VisitorVariant& var,
                iscore::Document *parent) override;


        void on_newDocument(iscore::Document* doc) override;
        void on_loadedDocument(iscore::Document* doc) override;

    protected:
        //void on_documentChanged() override;

    private:
};
