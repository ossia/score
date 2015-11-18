#pragma once
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include "ProcessWrapper.hpp"
#include <memory>
#include <Process/TimeValue.hpp>
#include <Editor/TimeValue.h>


class ConstraintModel;
class Process;
class OSSIAProcessElement;
namespace OSSIA
{
class Loop;
class TimeConstraint;
class StateElement;
class Scenario;
class TimeNode;
class TimeProcess;
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
