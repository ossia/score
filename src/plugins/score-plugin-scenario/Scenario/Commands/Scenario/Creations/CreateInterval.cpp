// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CreateInterval.hpp"

#include <Process/Process.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/model/EntityMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/model/path/ObjectPath.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/RandomNameProvider.hpp>


#include <vector>

namespace Scenario
{
namespace Command
{
CreateInterval::CreateInterval(
    const Scenario::ProcessModel& scenar,
    Id<StateModel> startState,
    Id<StateModel> endState,
    bool graph)
    : m_path{scenar}
    , m_createdName{RandomNameProvider::generateName<IntervalModel>()}
    , m_startStateId{std::move(startState)}
    , m_endStateId{std::move(endState)}
    , m_graphal{graph}
{
  auto ss = scenar.states.find(m_startStateId);
  if (ss != scenar.states.end())
    m_startStatePos = ss->heightPercentage();
  auto es = scenar.states.find(m_startStateId);
  if (es != scenar.states.end())
    m_endStatePos = es->heightPercentage();

  m_createdIntervalId = getStrongId(scenar.intervals);
}

void CreateInterval::undo(const score::DocumentContext& ctx) const
{
  auto& scenar = m_path.find(ctx);

  ScenarioCreate<IntervalModel>::undo(m_createdIntervalId, scenar);
  if (m_startStatePos != -1)
  {
    auto& sst = scenar.states.at(m_startStateId);
    if (sst.previousInterval())
      scenar.intervals.at(*sst.previousInterval()).requestHeightChange(m_startStatePos);
    else
      sst.setHeightPercentage(m_startStatePos);
  }
}

void CreateInterval::redo(const score::DocumentContext& ctx) const
{
  auto& scenar = m_path.find(ctx);
  auto& sst = scenar.states.at(m_startStateId);
  auto& est = scenar.states.at(m_endStateId);

  ScenarioCreate<IntervalModel>::redo(
      m_createdIntervalId, sst, est, sst.heightPercentage(), m_graphal, scenar);

  auto& itv = scenar.intervals.at(m_createdIntervalId);
  itv.metadata().setName(m_createdName);

  itv.requestHeightChange(est.heightPercentage());
}

void CreateInterval::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_createdName << m_createdIntervalId << m_startStateId
    << m_endStateId << m_startStatePos << m_endStatePos << m_graphal;
}

void CreateInterval::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_createdName >> m_createdIntervalId >> m_startStateId
      >> m_endStateId >> m_startStatePos >> m_endStatePos >> m_graphal;
}
}
}
