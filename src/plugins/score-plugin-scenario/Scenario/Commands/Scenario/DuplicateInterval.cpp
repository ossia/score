#include "DuplicateInterval.hpp"

#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>

#include <score/document/ChangeId.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/model/EntitySerialization.hpp>
namespace Scenario::Command
{

DuplicateInterval::DuplicateInterval(
    const Scenario::ProcessModel& parent,
    const IntervalModel& cst)
    : m_cmdStart{parent,
                 getStrongId(parent.states),
                 Scenario::startState(cst, parent).eventId(),
                 cst.heightPercentage() + 0.1}
    , m_cmdEnd{parent,
               Id<StateModel>{(int)getStrongId(parent.states) + 1},
               Scenario::endState(cst, parent).eventId(),
               cst.heightPercentage() + 0.1}
    , m_path{cst}
    , m_createdId{getStrongId(parent.intervals)}
{
}

DuplicateInterval::~DuplicateInterval() {}

void DuplicateInterval::undo(const score::DocumentContext& ctx) const
{
  auto& root = m_path.find(ctx);

  auto scenar = safe_cast<Scenario::ProcessModel*>(root.parent());
  scenar->intervals.remove(m_createdId);

  m_cmdEnd.undo(ctx);
  m_cmdStart.undo(ctx);
}

void DuplicateInterval::redo(const score::DocumentContext& ctx) const
{
  m_cmdStart.redo(ctx);
  m_cmdEnd.redo(ctx);
  auto& root = m_path.find(ctx);

  auto scenar = safe_cast<Scenario::ProcessModel*>(root.parent());

  auto obj = score::marshall<DataStream>(root);
  auto interval
      = new Scenario::IntervalModel{DataStream::Deserializer{obj}, ctx, scenar};
  score::IDocument::changeObjectId(*interval, m_createdId);

  interval->setStartState(m_cmdStart.createdState());
  interval->setEndState(m_cmdEnd.createdState());
  SetPreviousInterval(scenar->states.at(m_cmdEnd.createdState()), *interval);
  SetNextInterval(scenar->states.at(m_cmdStart.createdState()), *interval);
  interval->setHeightPercentage(root.heightPercentage() + 0.1);
  scenar->intervals.add(interval);
}

const Path<IntervalModel>& DuplicateInterval::intervalPath() const
{
  return m_path;
}

void DuplicateInterval::serializeImpl(DataStreamInput& s) const
{
  s << m_cmdStart.serialize() << m_cmdEnd.serialize() << m_path << m_createdId;
}

void DuplicateInterval::deserializeImpl(DataStreamOutput& s)
{
  QByteArray arr, arr2;
  s >> arr >> arr2 >> m_path >> m_createdId;
  m_cmdStart.deserialize(arr);
  m_cmdEnd.deserialize(arr2);
}
}
