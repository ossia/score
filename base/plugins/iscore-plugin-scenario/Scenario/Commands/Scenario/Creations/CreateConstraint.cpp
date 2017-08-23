// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <iscore/tools/RandomNameProvider.hpp>

#include <QDataStream>
#include <QList>
#include <QVector>
#include <QtGlobal>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <algorithm>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <vector>

#include "CreateConstraint.hpp"
#include <Process/Process.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/path/ObjectPath.hpp>
#include <iscore/model/Identifier.hpp>
#include <Dataflow/DocumentPlugin.hpp>

namespace Scenario
{
namespace Command
{
CreateConstraint::CreateConstraint(
    const Scenario::ProcessModel& scenar,
    Id<StateModel> startState,
    Id<StateModel> endState)
    : m_path{scenar}
    , m_createdName{RandomNameProvider::generateRandomName()}
    , m_startStateId{std::move(startState)}
    , m_endStateId{std::move(endState)}
{
    auto ss = scenar.states.find(m_startStateId);
    if(ss != scenar.states.end())
        m_startStatePos = ss->heightPercentage();
    auto es = scenar.states.find(m_startStateId);
    if(es != scenar.states.end())
        m_endStatePos = es->heightPercentage();

  m_createdConstraintId = getStrongId(scenar.constraints);

  auto& plug = iscore::IDocument::documentContext(scenar).model<ScenarioDocumentModel>();

  m_cables = getStrongIdRange<Process::Cable>(4, plug.cables);
}

void CreateConstraint::undo(const iscore::DocumentContext& ctx) const
{
  auto& scenar = m_path.find(ctx);
  auto& plug = ctx.model<ScenarioDocumentModel>();
  for(auto& cable : m_cables)
    plug.cables.remove(cable);


  ScenarioCreate<ConstraintModel>::undo(m_createdConstraintId, scenar);
  if(m_startStatePos != -1)
  {
      auto& sst = scenar.states.at(m_startStateId);
      if(sst.previousConstraint())
          updateConstraintVerticalPos(m_startStatePos, *sst.previousConstraint(), scenar);
      else
          sst.setHeightPercentage(m_startStatePos);
  }
}

void CreateConstraint::redo(const iscore::DocumentContext& ctx) const
{
  auto& scenar = m_path.find(ctx);
  auto& sst = scenar.states.at(m_startStateId);
  auto& est = scenar.states.at(m_endStateId);

  ScenarioCreate<ConstraintModel>::redo(
      m_createdConstraintId,
      sst,
      est,
      sst.heightPercentage(),
      scenar);

  auto& cst = scenar.constraints.at(m_createdConstraintId);
  cst.metadata().setName(m_createdName);

  updateConstraintVerticalPos(est.heightPercentage(), m_createdConstraintId, scenar);

  auto& plug = ctx.model<ScenarioDocumentModel>();

  {
    // Create a cable from the slider to the scenario
    auto cable = new Process::Cable{m_cables[0]};
    cable->setSource(&cst.slider);
    cable->setSink(&scenar.m_node);
    cable->setOutlet(0);
    cable->setInlet(0);
    plug.cables.add(cable);
  }

  // Connect constraint to slider and slider to this
  {
    auto cable = new Process::Cable{m_cables[1]};
    cable->setSource(&cst.node);
    cable->setSink(&cst.slider);
    cable->setOutlet(0);
    cable->setInlet(0);
    plug.cables.add(cable);
  }
  { // Messages
    auto cable = new Process::Cable{m_cables[2]};
    cable->setSource(&cst.node);
    cable->setSink(&scenar.m_node);
    cable->setOutlet(1);
    cable->setInlet(1);
    plug.cables.add(cable);
  }
  { // MIDI
    auto cable = new Process::Cable{m_cables[3]};
    cable->setSource(&cst.node);
    cable->setSink(&scenar.m_node);
    cable->setOutlet(2);
    cable->setInlet(2);
    plug.cables.add(cable);
  }
}

void CreateConstraint::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_createdName << m_createdConstraintId << m_startStateId
    << m_endStateId << m_startStatePos << m_endStatePos << m_cables;
}

void CreateConstraint::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_createdName >> m_createdConstraintId >> m_startStateId
    >> m_endStateId >> m_startStatePos >> m_endStatePos >> m_cables;
}
}
}
