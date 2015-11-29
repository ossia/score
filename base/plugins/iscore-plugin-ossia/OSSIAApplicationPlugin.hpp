#pragma once
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <Explorer/DocumentPlugin/ListeningState.hpp>

#include <Process/TimeValue.hpp>

namespace OSSIA
{
    class Device;
    class TimeConstraint;
}
namespace RecreateOnPlay
{
class ConstraintElement;
}
// TODO this should have "OSSIA Policies" : one would be the
// "basic" that corresponds to the default scenario.
// One would be the "distributed" policy which provides the
// same functionalities but for scenario executing on different computers.

class OSSIAApplicationPlugin final : public iscore::GUIApplicationContextPlugin
{
    public:
        OSSIAApplicationPlugin(iscore::Application& app);
        ~OSSIAApplicationPlugin();

        void populateMenus(iscore::MenubarManager*) override;

        iscore::DocumentDelegatePluginModel*loadDocumentPlugin(
                const QString& name,
                const VisitorVariant& var,
                iscore::Document* parent) override;

        void on_newDocument(iscore::Document* doc) override;
        void on_loadedDocument(iscore::Document* doc) override;

        RecreateOnPlay::ConstraintElement& baseConstraint() const;
        std::shared_ptr<OSSIA::Device> localDevice() const
        { return m_localDevice; }

    private:
        void on_play(bool, ::TimeValue t = ::TimeValue::zero() );
        void on_stop();

        void setupOSSIACallbacks();
        std::shared_ptr<OSSIA::Device> m_localDevice;
        std::shared_ptr<OSSIA::Device> m_remoteDevice;

        bool m_playing{false};

        ListeningState m_savedListening;
};
