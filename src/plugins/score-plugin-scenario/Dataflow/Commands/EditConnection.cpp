#include <Dataflow/Commands/EditConnection.hpp>

#include <score/model/path/PathSerialization.hpp>

namespace Dataflow
{
CreateCable::CreateCable(
    const Scenario::ScenarioDocumentModel& dp,
    Id<Process::Cable> theCable,
    Process::CableData dat)
    : m_model{dp}, m_cable{std::move(theCable)}, m_dat{std::move(dat)}
{
  SCORE_ASSERT(m_dat.source != m_dat.sink);
}

CreateCable::CreateCable(
    const Scenario::ScenarioDocumentModel& dp,
    Id<Process::Cable> theCable,
    const Process::Cable& cable)
    : m_model{dp}, m_cable{std::move(theCable)}
{
  m_dat.source = cable.source();
  m_dat.sink = cable.sink();
  m_dat.type = cable.type();

  SCORE_ASSERT(m_dat.source != m_dat.sink);
}

void CreateCable::undo(const score::DocumentContext& ctx) const
{
  auto ext = m_model.extend(m_cable);
  m_dat.source.find(ctx).removeCable(ext);
  m_dat.sink.find(ctx).removeCable(ext);
  m_model.find(ctx).cables.remove(m_cable);
}

void CreateCable::redo(const score::DocumentContext& ctx) const
{
  auto& model = m_model.find(ctx);
  auto c = new Process::Cable{m_cable, m_dat, &model};

  model.cables.add(c);
  auto ext = m_model.extend(m_cable);
  m_dat.source.find(ctx).addCable(*c);
  m_dat.sink.find(ctx).addCable(*c);
}

void CreateCable::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_cable << m_dat;
}

void CreateCable::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_cable >> m_dat;
}

UpdateCable::UpdateCable(
    const Process::Cable& cable,
    Process::CableType newDat)
    : m_model{cable}, m_new{newDat}
{
  m_old = cable.type();
}

void UpdateCable::undo(const score::DocumentContext& ctx) const
{
  m_model.find(ctx).setType(m_old);
}

void UpdateCable::redo(const score::DocumentContext& ctx) const
{
  m_model.find(ctx).setType(m_new);
}

void UpdateCable::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_old << m_new;
}

void UpdateCable::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_old >> m_new;
}

RemoveCable::RemoveCable(
    const Scenario::ScenarioDocumentModel& dp,
    const Process::Cable& c)
    : m_model{dp}, m_cable{c.id()}
{
  m_data.type = c.type();
  m_data.source = c.source();
  m_data.sink = c.sink();
}

void RemoveCable::undo(const score::DocumentContext& ctx) const
{
  auto& model = m_model.find(ctx);
  auto cbl = new Process::Cable{m_cable, m_data, &model};
  model.cables.add(cbl);
  cbl->source().find(ctx).addCable(*cbl);
  cbl->sink().find(ctx).addCable(*cbl);
}

void RemoveCable::redo(const score::DocumentContext& ctx) const
{
  auto& cables = m_model.find(ctx).cables;
  auto cable_it = cables.find(m_cable);
  if (cable_it != cables.end())
  {
    auto& cable = *cable_it;
    cable.source().find(ctx).removeCable(cable);
    cable.sink().find(ctx).removeCable(cable);
    m_model.find(ctx).cables.remove(m_cable);
  }
}

void RemoveCable::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_cable << m_data;
}

void RemoveCable::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_cable >> m_data;
}
}
