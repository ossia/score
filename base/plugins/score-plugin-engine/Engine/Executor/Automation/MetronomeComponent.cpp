//// This is an open source non-commercial project. Dear PVS-Studio, please check it.
//// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
//#include "MetronomeComponent.hpp"
//#include <Engine/Executor/DocumentPlugin.hpp>
//#include <Engine/Executor/ExecutorContext.hpp>
//#include <Engine/score2OSSIA.hpp>
//#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
//#include <ossia/editor/automation/automation.hpp>
//#include <Engine/CurveConversion.hpp>
//namespace Metronome
//{
//namespace RecreateOnPlay
//{

//Component::Component(
//    ::Engine::Execution::IntervalComponent& parentInterval,
//    ::Metronome::ProcessModel& element,
//    const ::Engine::Execution::Context& ctx,
//    const Id<score::Component>& id,
//    QObject* parent)
//  : ::Engine::Execution::
//      ProcessComponent_T<Metronome::ProcessModel, ossia::metronome>{
//        parentInterval,
//        element,
//        ctx,
//        id, "Executor::MetronomeComponent", parent}
//{
//  m_ossia_process = std::make_shared<ossia::metronome>();

//  con(element, &Metronome::ProcessModel::addressChanged,
//      this, [this] (const auto&) { this->recompute(); });
//  con(element, &Metronome::ProcessModel::curveChanged,
//      this, [this] () { this->recompute(); });

//  recompute();
//}

//void Component::recompute()
//{
//  auto dest = Engine::score_to_ossia::makeDestination(
//        system().devices.list(),
//        State::AddressAccessor{process().address()});

//  if (dest)
//  {
//    auto& d = *dest;
//    auto curve = on_curveChanged();

//    if (curve)
//    {
//      system().executionQueue.enqueue(
//            [proc=std::dynamic_pointer_cast<ossia::metronome>(m_ossia_process)
//            ,curve
//            ,d_=d]
//      {
//        proc->set_destination(std::move(d_));
//        proc->set_curve(curve);
//      });
//      return;
//    }
//  }

//  // If something did not work out
//  system().executionQueue.enqueue(
//        [proc=std::dynamic_pointer_cast<ossia::metronome>(m_ossia_process)]
//  {
//    proc->clean();
//  });
//}

//std::shared_ptr<ossia::curve<double,float>>
//Component::on_curveChanged()
//{
//  using namespace ossia;

//  const double min = 1000;//process().min();
//  const double max = 1000000;//process().max();

//  auto scale_x = [](double val) -> double { return val; };
//  auto scale_y = [=](double val) -> float { return val * (max - min) + min; };

//  auto segt_data = process().curve().sortedSegments();
//  if (segt_data.size() != 0)
//  {
//    return Engine::score_to_ossia::curve<double, float>(
//          scale_x, scale_y, segt_data, {});
//  }
//  else
//  {
//    return {};
//  }
//}

//}
//}

