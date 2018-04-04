// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MetronomeComponent.hpp"
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/score2OSSIA.hpp>
#include <Engine/CurveConversion.hpp>
#include <ossia/dataflow/nodes/metronome.hpp>
namespace Metronome
{
namespace RecreateOnPlay
{
using metronome = ossia::nodes::metronome;
Component::Component(
    ::Metronome::ProcessModel& element,
    const ::Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
  : ::Engine::Execution::
      ProcessComponent_T<Metronome::ProcessModel, ossia::node_process>{
        element,
        ctx,
        id, "Executor::MetronomeComponent", parent}
{
  auto node = std::make_shared<metronome>();
  this->node = node;
  m_ossia_process = std::make_shared<ossia::nodes::metronome_process>(node);

  con(element, &Metronome::ProcessModel::curveChanged,
      this, [this] { this->recompute(); });

  recompute();
}

Component::~Component()
{
}

std::shared_ptr<ossia::curve<double,float>>
Component::on_curveChanged()
{
  using namespace ossia;

  const double min = 1000;//process().min();
  const double max = 1000000;//process().max();

  auto scale_x = [](double val) -> double { return val; };
  auto scale_y = [=](double val) -> float { return val * (max - min) + min; };

  auto segt_data = process().curve().sortedSegments();
  if (segt_data.size() != 0)
  {
    return Engine::score_to_ossia::curve<double, float>(
          scale_x, scale_y, segt_data, {});
  }
  else
  {
    return {};
  }
}
void Component::recompute()
{
  auto curve = on_curveChanged();

  if (curve)
  {
    system().executionQueue.enqueue(
          [proc=std::dynamic_pointer_cast<ossia::nodes::metronome>(OSSIAProcess().node)
          ,curve]
    {
      proc->set_curve(curve);
    });
    return;
  }
}

}
}
