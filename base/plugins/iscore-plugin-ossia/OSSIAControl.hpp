#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>

namespace OSSIA
{
    class Device;
    class TimeConstraint;
}
class OSSIAControl : public iscore::PluginControlInterface
{
    public:
        OSSIAControl(iscore::Presenter* pres);

        void populateMenus(iscore::MenubarManager*);

        iscore::DocumentDelegatePluginModel*loadDocumentPlugin(
                const QString& name,
                const VisitorVariant& var,
                iscore::DocumentModel* parent);

        void on_newDocument(iscore::Document* doc) override;
        void on_loadedDocument(iscore::Document* doc) override;

    protected:
        void on_documentChanged();

    private:
        OSSIA::TimeConstraint& baseConstraint() const;
        std::shared_ptr<OSSIA::Device> m_localDevice;
};
