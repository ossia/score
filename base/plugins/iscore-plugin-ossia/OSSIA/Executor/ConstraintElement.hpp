#pragma once
#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/editor/state/state_element.hpp>
#include <Process/TimeValue.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <QObject>
#include <unordered_map>
#include <memory>

#include <OSSIA/Executor/ProcessElement.hpp>
#include <OSSIA/Executor/ProcessWrapper.hpp>
#include <iscore_plugin_ossia_export.h>

namespace Process { class ProcessModel; }
namespace iscore
{
struct DocumentContext;
}
namespace ossia
{
class loop;
class time_constraint;
}
namespace Scenario
{
class ConstraintModel;
}

namespace RecreateOnPlay
{
struct Context;
class DocumentPlugin;
class ISCORE_PLUGIN_OSSIA_EXPORT ConstraintElement final : public QObject
{
    public:
        ConstraintElement(
                std::shared_ptr<ossia::time_constraint> ossia_cst,
                Scenario::ConstraintModel& iscore_cst,
                const Context &ctx,
                QObject* parent);

        std::shared_ptr<ossia::time_constraint> OSSIAConstraint() const;
        Scenario::ConstraintModel& iscoreConstraint() const;

        void play(TimeValue t = TimeValue::zero());
        void pause();
        void resume();
        void stop();

        void executionStarted();
        void executionStopped();

    private:
        void on_processAdded(
                const Process::ProcessModel& iscore_proc);
        void constraintCallback(
                ossia::time_value position,
                ossia::time_value date,
                const ossia::state& state);

        Scenario::ConstraintModel& m_iscore_constraint;
        std::shared_ptr<ossia::time_constraint> m_ossia_constraint;

        std::unordered_map<Id<Process::ProcessModel>, OSSIAProcess> m_processes;

        std::shared_ptr<ossia::loop> m_loop;

        ossia::time_value m_offset;

        const RecreateOnPlay::Context& m_ctx;
};

}
