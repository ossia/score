#pragma once
#include <OSSIA/Executor/ProcessElement.hpp>
#include <memory>
#include <Loop/LoopProcessModel.hpp>
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

namespace Loop
{
namespace RecreateOnPlay
{
// TODO see if this can be used for the base scenario model too.
class Component final :
        public ::RecreateOnPlay::ProcessComponent_T<Loop::ProcessModel>
{
        COMPONENT_METADATA("77b987ae-7bc8-4273-aa9c-9e4ba53a053d")
    public:
        Component(
                ::RecreateOnPlay::ConstraintElement& parentConstraint,
                ::Loop::ProcessModel& element,
                const ::RecreateOnPlay::Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent);

        virtual ~Component();

        void stop() override;

    private:
        void startConstraintExecution(const Id<Scenario::ConstraintModel>&);
        void stopConstraintExecution(const Id<Scenario::ConstraintModel>&);


    private:
        ::RecreateOnPlay::ConstraintElement* m_ossia_constraint{};

        ::RecreateOnPlay::TimeNodeElement* m_ossia_startTimeNode{};
        ::RecreateOnPlay::TimeNodeElement* m_ossia_endTimeNode{};

        ::RecreateOnPlay::EventElement* m_ossia_startEvent{};
        ::RecreateOnPlay::EventElement* m_ossia_endEvent{};

        ::RecreateOnPlay::StateElement* m_ossia_startState{};
        ::RecreateOnPlay::StateElement* m_ossia_endState{};
};

using ComponentFactory = ::RecreateOnPlay::ProcessComponentFactory_T<Component>;

}
}

ISCORE_COMPONENT_FACTORY(RecreateOnPlay::ProcessComponentFactory, Loop::RecreateOnPlay::ComponentFactory)
