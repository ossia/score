#pragma once
#include <Explorer/DocumentPlugin/ListeningState.hpp>
#include <Process/TimeValue.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <QString>
#include <memory>

#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>

namespace iscore {

class Document;
class MenubarManager;
}  // namespace iscore
struct VisitorVariant;

namespace OSSIA
{
    class Device;
}
namespace RecreateOnPlay
{
class ConstraintElement;
}
// TODO this should have "OSSIA Policies" : one would be the
// "basic" that corresponds to the default scenario.
// One would be the "distributed" policy which provides the
// same functionalities but for scenario executing on different computers.

class OSSIAApplicationPlugin final :
        public QObject,
        public iscore::GUIApplicationContextPlugin
{
    public:
        OSSIAApplicationPlugin(const iscore::ApplicationContext& app);
        ~OSSIAApplicationPlugin();

        void populateMenus(iscore::MenubarManager*) override;

        bool handleStartup() override;

        void on_newDocument(iscore::Document* doc) override;
        void on_loadedDocument(iscore::Document* doc) override;

        void on_documentChanged(
                iscore::Document* olddoc,
                iscore::Document* newdoc) override;

        RecreateOnPlay::ConstraintElement& baseConstraint() const;
        const std::shared_ptr<OSSIA::Device>& localDevice() const
        { return m_localDevice; }

    private:
        void on_play(bool, ::TimeValue t = ::TimeValue::zero() );
        void on_stop();
        void on_init();

        void setupOSSIACallbacks();
        std::shared_ptr<OSSIA::Device> m_localDevice;
        std::shared_ptr<OSSIA::Device> m_remoteDevice;

        bool m_playing{false};
};
