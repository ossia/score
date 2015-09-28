#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>

namespace OSSIA
{
    class Device;
    class TimeConstraint;
}
class OSSIAConstraintElement;
// TODO this should have "OSSIA Policies" : one would be the
// "basic" that corresponds to the default scenario.
// One would be the "distributed" policy which provides the
// same functionalities but for scenario executing on different computers.

class OSSIAControl : public iscore::PluginControlInterface
{
    public:
        OSSIAControl(iscore::Presenter* pres);
        ~OSSIAControl();

        void populateMenus(iscore::MenubarManager*) override;

        iscore::DocumentDelegatePluginModel*loadDocumentPlugin(
                const QString& name,
                const VisitorVariant& var,
                iscore::DocumentModel* parent) override;

        void on_newDocument(iscore::Document* doc) override;
        void on_loadedDocument(iscore::Document* doc) override;

    protected:
        void on_documentChanged() override;

    private:
        OSSIAConstraintElement &baseConstraint() const;
        std::shared_ptr<OSSIA::Device> m_localDevice;

        bool m_playing{false};
};
