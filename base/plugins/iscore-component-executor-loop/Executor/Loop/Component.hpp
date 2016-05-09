#pragma once
#include <OSSIA/Executor/ProcessElement.hpp>
#include <memory>
#include <iscore/tools/SettableIdentifier.hpp>

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
namespace Scenario
{
class ConstraintModel;
}

namespace RecreateOnPlay
{
class ConstraintElement;

namespace Loop
{
// TODO see if this can be used for the base scenario model too.
class Component final : public ProcessComponent
{
    public:
        Component(
                ConstraintElement& parentConstraint,
                ::Loop::ProcessModel& element,
                const Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent);

        virtual ~Component();

        void stop() override;

    private:
        void startConstraintExecution(const Id<Scenario::ConstraintModel>&);
        void stopConstraintExecution(const Id<Scenario::ConstraintModel>&);


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
};


class ComponentFactory final :
        public ProcessComponentFactory
{
        ISCORE_COMPONENT_FACTORY(RecreateOnPlay::ProcessComponentFactory, RecreateOnPlay::Loop::ComponentFactory)
    public:
        virtual ~ComponentFactory();
        ProcessComponent* make(
                ConstraintElement& cst,
                Process::ProcessModel& proc,
                const Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent) const override;

        const ConcreteFactoryKey& concreteFactoryKey() const override;

        bool matches(
                Process::ProcessModel&,
                const DocumentPlugin&,
                const iscore::DocumentContext &) const override;
};
}
}
