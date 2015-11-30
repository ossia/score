#pragma once
#include <OSSIA/RecreateOnPlayDocumentPlugin/ProcessElement.hpp>
#include <memory>

class ConstraintModel;
class DeviceList;
class Process;
class QObject;
namespace Loop {
class ProcessModel;
}  // namespace Loop
namespace OSSIA {
class Loop;
class TimeProcess;
}  // namespace OSSIA
namespace RecreateOnPlay {
class EventElement;
class StateElement;
class TimeNodeElement;
}  // namespace RecreateOnPlay
#include <iscore/tools/SettableIdentifier.hpp>


namespace RecreateOnPlay
{
class ConstraintElement;

// TODO see if this can be used for the base scenario model too.
class LoopElement final : public ProcessElement
{
    public:
        LoopElement(
                ConstraintElement& parentConstraint,
                Loop::ProcessModel& element,
                QObject* parent);

        virtual ~LoopElement();

        std::shared_ptr<OSSIA::TimeProcess> OSSIAProcess() const override;
        std::shared_ptr<OSSIA::Loop> scenario() const;

        Process& iscoreProcess() const override;

        void stop() override;

    private:
        void startConstraintExecution(const Id<ConstraintModel>&);

        void stopConstraintExecution(const Id<ConstraintModel>&);


    private:
        ConstraintElement* m_ossia_constraint{};

        TimeNodeElement* m_ossia_startTimeNode{};
        TimeNodeElement* m_ossia_endTimeNode{};

        EventElement* m_ossia_startEvent{};
        EventElement* m_ossia_endEvent{};

        StateElement* m_ossia_startState{};
        StateElement* m_ossia_endState{};

        std::shared_ptr<OSSIA::Loop> m_ossia_loop;
        Loop::ProcessModel& m_iscore_loop;

        const DeviceList& m_deviceList;
};
}
