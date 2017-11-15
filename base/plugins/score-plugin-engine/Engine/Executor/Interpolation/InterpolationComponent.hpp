//#pragma once
//#include <ossia/editor/automation/automation.hpp>
//#include <ossia/network/domain/domain.hpp>
//#include <Engine/Executor/ProcessComponent.hpp>
//#include <Interpolation/InterpolationProcess.hpp>

//namespace ossia
//{
//class curve_abstract;
//}

//namespace Device
//{
//class DeviceList;
//}

//namespace Interpolation
//{
//namespace Executor
//{
//class Component final
//    : public ::Engine::Execution::
//          ProcessComponent_T<Interpolation::ProcessModel, ossia::automation>
//{
//  COMPONENT_METADATA("1901fca9-60db-45a2-9c13-8f8e79d5850f")
//public:
//  Component(
//      ::Engine::Execution::IntervalComponent& parentInterval,
//      Interpolation::ProcessModel& element,
//      const ::Engine::Execution::Context& ctx,
//      const Id<score::Component>& id,
//      QObject* parent);

//private:
//  void recompute();
//  void rebuildCurve();

//  ossia::behavior on_curveChanged(
//      ossia::val_type,
//      const optional<ossia::destination>& d);

//  template <typename T>
//  std::shared_ptr<ossia::curve_abstract>
//  on_curveChanged_impl(double min, double max, double s, double e, const optional<ossia::destination>& d);
//};
//using ComponentFactory
//    = ::Engine::Execution::ProcessComponentFactory_T<Component>;
//}
//}

//SCORE_CONCRETE_COMPONENT_FACTORY(
//    Engine::Execution::ProcessComponentFactory,
//    Interpolation::RecreateOnPlay::ComponentFactory)
