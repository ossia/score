#pragma once
#include <OSSIA/Executor/ProcessElement.hpp>
#include <memory>

class ConstraintModel;
class DeviceList;
namespace Process { class ProcessModel; }
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
class LoopElement final : public ProcessComponent
{
    public:
        LoopElement(
                ConstraintElement& parentConstraint,
                Loop::ProcessModel& element,
                const Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent);

        virtual ~LoopElement();

        std::shared_ptr<OSSIA::TimeProcess> OSSIAProcess() const override;
        std::shared_ptr<OSSIA::Loop> scenario() const;

        Process::ProcessModel& iscoreProcess() const override;

        void stop() override;

    private:
        void startConstraintExecution(const Id<ConstraintModel>&);

        void stopConstraintExecution(const Id<ConstraintModel>&);


    private:
        const Key &key() const override;
        const Context& m_ctx;

        ConstraintElement* m_ossia_constraint{};

        TimeNodeElement* m_ossia_startTimeNode{};
        TimeNodeElement* m_ossia_endTimeNode{};

        EventElement* m_ossia_startEvent{};
        EventElement* m_ossia_endEvent{};

        StateElement* m_ossia_startState{};
        StateElement* m_ossia_endState{};

        std::shared_ptr<OSSIA::Loop> m_ossia_loop;
        Loop::ProcessModel& m_iscore_loop;
};


class LoopComponentFactory final :
        public ProcessComponentFactory
{
    public:
        virtual ~LoopComponentFactory();
        virtual ProcessComponent* make(
                ConstraintElement& cst,
                Process::ProcessModel& proc,
                const Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent) const override;

        const factory_key_type& key_impl() const override;

        bool matches(
                Process::ProcessModel&,
                const DocumentPlugin&,
                const iscore::DocumentContext &) const override;
};
}
