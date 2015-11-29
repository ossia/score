#pragma once
#include <Editor/TimeValue.h>
#include <Process/TimeValue.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <qobject.h>
#include <map>
#include <memory>

#include "ProcessWrapper.hpp"

class ConstraintModel;
class Process;

namespace OSSIA
{
class Loop;
class StateElement;
class TimeConstraint;
}

class OSSIAConstraintElement final : public QObject
{
    public:
        OSSIAConstraintElement(
                std::shared_ptr<OSSIA::TimeConstraint> ossia_cst,
                ConstraintModel& iscore_cst,
                QObject* parent);

        std::shared_ptr<OSSIA::TimeConstraint> OSSIAConstraint() const;
        ConstraintModel& iscoreConstraint() const;

        void play(TimeValue t = TimeValue::zero());
        void stop();

        void executionStarted();
        void executionStopped();

        void recreate();
        void clear();

    private:
        void on_processAdded(const Process& id);
        void on_processRemoved(const Process& id);

        void on_loopingChanged(bool);

        void constraintCallback(
                const OSSIA::TimeValue& position,
                const OSSIA::TimeValue& date,
                const std::shared_ptr<OSSIA::StateElement>& state);

        ConstraintModel& m_iscore_constraint;
        std::shared_ptr<OSSIA::TimeConstraint> m_ossia_constraint;

        std::map<Id<Process>, OSSIAProcess> m_processes;

        std::shared_ptr<OSSIA::Loop> m_loop;

        OSSIA::TimeValue m_offset;
};
