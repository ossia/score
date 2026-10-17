#include "PatternTimeGrid.hpp"

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/tools/Bind.hpp>

#include <Patternist/PatternModel.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Patternist::TimeGridWatcher)

namespace Patternist
{
TimeGridWatcher::TimeGridWatcher(const ProcessModel& proc, QObject* parent)
    : QObject{parent}
    , m_process{proc}
{
  con(proc, &ProcessModel::timeGridChanged, this, &TimeGridWatcher::update);
  con(proc, &ProcessModel::startOffsetChanged, this, &TimeGridWatcher::update);
  update();
}

void TimeGridWatcher::update()
{
  for(auto& c : m_connections)
    QObject::disconnect(c);
  m_connections.clear();

  TimeVal delta = TimeVal::zero();
  const Scenario::IntervalModel* found{};
  for(auto itv = Scenario::closestParentInterval(m_process.parent()); itv;
      itv = Scenario::closestParentInterval(itv->parent()))
  {
    // Any interval up to the one found can gain a signature and become it.
    m_connections.push_back(connect(
        itv, &Scenario::IntervalModel::hasTimeSignatureChanged, this,
        &TimeGridWatcher::update));
    m_connections.push_back(connect(
        itv, &Scenario::IntervalModel::timeSignaturesChanged, this,
        &TimeGridWatcher::update));

    found = itv;
    if(itv->hasTimeSignature() || itv->tempoCurve())
      break;
    delta += itv->date();
  }

  TimeGrid g;
  if(found && found->hasTimeSignature())
    for(const auto& [date, sig] : found->timeSignatureMap())
      g.signatures.emplace_back(date, sig);
  g.playStart = delta;
  g.origin = (m_process.flags() & Process::ProcessFlags::HandlesLooping)
                 ? delta
                 : delta - m_process.startOffset();

  if(!(g == m_grid))
  {
    m_grid = std::move(g);
    changed(m_grid);
  }
}
}
