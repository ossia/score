#include <Dataflow/Commands/EditConnection.hpp>

#include <score/model/path/PathSerialization.hpp>

namespace Dataflow
{
CreateCable::CreateCable(
    const Scenario::ScenarioDocumentModel& dp,
    Id<Process::Cable> theCable,
    Process::CableType type,
    const Process::Port& source,
    const Process::Port& sink)
  : m_model{dp}
  , m_cable{std::move(theCable)}
  , m_dat{type, source, sink}
{
  SCORE_ASSERT(m_dat.source != m_dat.sink);

  if(source.type() == Process::PortType::Audio)
  {
    m_previousPropagate = static_cast<const Process::AudioOutlet&>(source).propagate();
  }
}

void CreateCable::undo(const score::DocumentContext& ctx) const
{
  auto ext = m_model.extend(m_cable);
  auto& source = m_dat.source.find(ctx);
  source.removeCable(ext);
  m_dat.sink.find(ctx).removeCable(ext);
  m_model.find(ctx).cables.remove(m_cable);

  if(m_previousPropagate)
  {
    static_cast<Process::AudioOutlet&>(source).setPropagate(true);
  }
}

void CreateCable::redo(const score::DocumentContext& ctx) const
{
  auto& model = m_model.find(ctx);
  auto c = new Process::Cable{m_cable, m_dat, &model};

  model.cables.add(c);
  auto ext = m_model.extend(m_cable);
  auto& source = m_dat.source.find(ctx);
  source.addCable(*c);
  m_dat.sink.find(ctx).addCable(*c);

  if(m_previousPropagate)
  {
    static_cast<Process::AudioOutlet&>(source).setPropagate(false);
  }
}

void CreateCable::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_cable << m_dat << m_previousPropagate;
}

void CreateCable::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_cable >> m_dat >> m_previousPropagate;
}

UpdateCable::UpdateCable(const Process::Cable& cable, Process::CableType newDat)
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

RemoveCable::RemoveCable(const Scenario::ScenarioDocumentModel& dp, const Process::Cable& c)
    : m_model{dp}, m_cable{c.id()}
{
  m_data.type = c.type();
  m_data.source = c.source();
  m_data.sink = c.sink();

  auto& source = c.source().find(dp.context());
  if(source.type() == Process::PortType::Audio)
  {
    m_previousPropagate = static_cast<const Process::AudioOutlet&>(source).propagate();
  }
}

void RemoveCable::undo(const score::DocumentContext& ctx) const
{
  auto& model = m_model.find(ctx);
  auto cbl = new Process::Cable{m_cable, m_data, &model};
  model.cables.add(cbl);
  auto& source = cbl->source().find(ctx);
  source.addCable(*cbl);
  cbl->sink().find(ctx).addCable(*cbl);

  if(m_previousPropagate && !m_previousPropagate)
  {
    static_cast<Process::AudioOutlet&>(source).setPropagate(false);
  }
}

void RemoveCable::redo(const score::DocumentContext& ctx) const
{
  auto& cables = m_model.find(ctx).cables;
  auto cable_it = cables.find(m_cable);
  if (cable_it != cables.end())
  {
    auto& cable = *cable_it;
    auto& source = cable.source().find(ctx);
    source.removeCable(cable);
    cable.sink().find(ctx).removeCable(cable);
    m_model.find(ctx).cables.remove(m_cable);

    if(m_previousPropagate && source.cables().size() == 0)
    {
      static_cast<Process::AudioOutlet&>(source).setPropagate(true);
    }
  }
}

void RemoveCable::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_cable << m_data << m_previousPropagate;
}

void RemoveCable::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_cable >> m_data >> m_previousPropagate;
}
}
