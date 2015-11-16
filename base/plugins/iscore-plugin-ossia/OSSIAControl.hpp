#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
namespace OSSIA
{
    class Device;
    class TimeConstraint;
}
class OSSIAConstraintElement;
class OSSIABaseScenarioElement;
// TODO this should have "OSSIA Policies" : one would be the
// "basic" that corresponds to the default scenario.
// One would be the "distributed" policy which provides the
// same functionalities but for scenario executing on different computers.

class OSSIAControl final : public iscore::PluginControlInterface
{
    public:
        OSSIAControl(iscore::Application& app);
        ~OSSIAControl();

        void populateMenus(iscore::MenubarManager*) override;

        iscore::DocumentDelegatePluginModel*loadDocumentPlugin(
                const QString& name,
                const VisitorVariant& var,
                iscore::DocumentModel* parent) override;

        void on_newDocument(iscore::Document* doc) override;
        void on_loadedDocument(iscore::Document* doc) override;

        OSSIAConstraintElement& baseConstraint() const;
        OSSIABaseScenarioElement& baseScenario() const;
        std::shared_ptr<OSSIA::Device> localDevice() const
        { return m_localDevice; }

    private:
        void on_play(bool);
        void on_stop();

        void setupOSSIACallbacks();
        std::shared_ptr<OSSIA::Device> m_localDevice;
        std::shared_ptr<OSSIA::Device> m_remoteDevice;

        bool m_playing{false};

        ListeningState m_savedListening;
};
