//#pragma once
//#include <ossia/editor/automation/automation.hpp>
//#include <ossia/network/value/value.hpp>
//#include <Automation/Metronome/MetronomeModel.hpp>
//#include <Engine/Executor/ProcessComponent.hpp>
//#include <ossia/editor/scenario/time_interval.hpp>
//#include <ossia/editor/curve/curve_segment/easing.hpp>
//#include <ossia/editor/automation/metronome.hpp>
//#include <memory>

//namespace Device
//{
//class DeviceList;
//}

//namespace Metronome
//{
//namespace RecreateOnPlay
//{
//class Component final
//    : public ::Engine::Execution::
//    ProcessComponent_T<Metronome::ProcessModel, ossia::metronome>
//{
//    COMPONENT_METADATA("e3777540-ba8a-4ffa-a7f3-d95ae0d712a7")
//    public:
//      Component(
//        ::Engine::Execution::IntervalComponent& parentInterval,
//        Metronome::ProcessModel& element,
//        const ::Engine::Execution::Context& ctx,
//        const Id<score::Component>& id,
//        QObject* parent);

//  private:
//    void recompute();

//    std::shared_ptr<ossia::curve<double,float>> on_curveChanged();
//};
//using ComponentFactory
//= ::Engine::Execution::ProcessComponentFactory_T<Component>;
//}
//}

//SCORE_CONCRETE_COMPONENT_FACTORY(
//    Engine::Execution::ProcessComponentFactory,
//    Metronome::RecreateOnPlay::ComponentFactory)
