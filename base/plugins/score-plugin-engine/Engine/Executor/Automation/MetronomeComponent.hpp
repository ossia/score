#pragma once
#include <Automation/Metronome/MetronomeModel.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <ossia/editor/curve/curve.hpp>

namespace Device
{
class DeviceList;
}

namespace Metronome
{
namespace RecreateOnPlay
{
class Component final
    : public ::Engine::Execution::
    ProcessComponent_T<Metronome::ProcessModel, ossia::node_process>
{
    COMPONENT_METADATA("37ff95e2-8450-4a08-b7ec-9bb0c724ce4c")
    public:
      Component(
        Metronome::ProcessModel& element,
        const ::Engine::Execution::Context& ctx,
        const Id<score::Component>& id,
        QObject* parent);
      ~Component() override;

private:
    std::shared_ptr<ossia::curve<double, float> > on_curveChanged();
    void recompute();

};
using ComponentFactory
= ::Engine::Execution::ProcessComponentFactory_T<Component>;
}
}


SCORE_CONCRETE_COMPONENT_FACTORY(
    Engine::Execution::ProcessComponentFactory,
    Metronome::RecreateOnPlay::ComponentFactory)
