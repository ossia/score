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
namespace ossia
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

        void on_play(bool, ::TimeValue t = ::TimeValue::zero() );
        void on_play(Scenario::ConstraintModel&, bool, ::TimeValue t = ::TimeValue::zero() );
        void on_record(::TimeValue t);

    private:
        void on_stop();
        void on_init();

        std::unique_ptr<RecreateOnPlay::ClockManager> makeClock(const RecreateOnPlay::Context&);

        RecreateOnPlay::PlayContextMenu m_playActions;

        std::unique_ptr<RecreateOnPlay::ClockManager> m_clock;
        bool m_playing{false};
};
