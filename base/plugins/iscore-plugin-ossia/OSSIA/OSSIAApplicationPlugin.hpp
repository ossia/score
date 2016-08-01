#pragma once

#include <Process/TimeValue.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <QString>
#include <memory>

#include <OSSIA/Executor/ContextMenu/PlayContextMenu.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore_plugin_ossia_export.h>
namespace iscore {

class Document;
}  // namespace iscore
struct VisitorVariant;

namespace Scenario
{ class ConstraintModel; }
namespace OSSIA
{
    class Device;
}
namespace RecreateOnPlay
{
class ClockManager;
struct Context;
class ConstraintElement;
}
// TODO this should have "OSSIA Policies" : one would be the
// "basic" that corresponds to the default scenario.
// One would be the "distributed" policy which provides the
// same functionalities but for scenario executing on different computers.

class ISCORE_PLUGIN_OSSIA_EXPORT OSSIAApplicationPlugin final :
        public QObject,
        public iscore::GUIApplicationContextPlugin
{
        Q_OBJECT
    public:
        OSSIAApplicationPlugin(
                const iscore::GUIApplicationContext& app);
        ~OSSIAApplicationPlugin();

        bool handleStartup() override;

        void on_newDocument(iscore::Document* doc) override;
        void on_loadedDocument(iscore::Document* doc) override;

        void on_documentChanged(
                iscore::Document* olddoc,
                iscore::Document* newdoc) override;

        const std::shared_ptr<OSSIA::Device>& localDevice() const
        { return m_localDevice; }

        void on_play(bool, ::TimeValue t = ::TimeValue::zero() );
        void on_play(Scenario::ConstraintModel&, bool, ::TimeValue t = ::TimeValue::zero() );
        void on_record(::TimeValue t);

    signals:
        void sig_play(bool, ::TimeValue);
        void sig_stop();
    private:
        void on_stop();
        void on_init();

        void setupOSSIACallbacks();

        std::unique_ptr<RecreateOnPlay::ClockManager> makeClock(const RecreateOnPlay::Context&);

        std::shared_ptr<OSSIA::Device> m_localDevice;
        std::shared_ptr<OSSIA::Device> m_remoteDevice;

        RecreateOnPlay::PlayContextMenu m_playActions;

        std::unique_ptr<RecreateOnPlay::ClockManager> m_clock;
        bool m_playing{false}, m_paused{false};
};
