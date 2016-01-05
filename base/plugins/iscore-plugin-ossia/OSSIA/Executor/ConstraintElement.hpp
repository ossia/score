#pragma once
#include <Editor/TimeValue.h>
#include <Process/TimeValue.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <QObject>
#include <map>
#include <memory>

#include <OSSIA/Executor/ProcessElement.hpp>
#include <OSSIA/Executor/ProcessWrapper.hpp>

class ConstraintModel;
namespace Process { class ProcessModel; }
namespace iscore
{
struct DocumentContext;
}
namespace OSSIA
{
class Loop;
class StateElement;
class TimeConstraint;
}

namespace RecreateOnPlay
{
struct Context;
class DocumentPlugin;
class ConstraintElement final : public QObject
{
    public:
        ConstraintElement(
                std::shared_ptr<OSSIA::TimeConstraint> ossia_cst,
                ConstraintModel& iscore_cst,
                const Context &ctx,
                QObject* parent);

        std::shared_ptr<OSSIA::TimeConstraint> OSSIAConstraint() const;
        ConstraintModel& iscoreConstraint() const;

        void play(TimeValue t = TimeValue::zero());
        void stop();

        void executionStarted();
        void executionStopped();

    private:
        void on_processAdded(
                const Process::ProcessModel& iscore_proc);
        void constraintCallback(
                const OSSIA::TimeValue& position,
                const OSSIA::TimeValue& date,
                const std::shared_ptr<OSSIA::StateElement>& state);

        ConstraintModel& m_iscore_constraint;
        std::shared_ptr<OSSIA::TimeConstraint> m_ossia_constraint;

        std::map<Id<Process::ProcessModel>, OSSIAProcess> m_processes;

        std::shared_ptr<OSSIA::Loop> m_loop;

        OSSIA::TimeValue m_offset;

        const RecreateOnPlay::Context& m_ctx;
};
}
