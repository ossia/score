#include <Process/Commands/EditPort.hpp>
#include <Process/Dataflow/Port.hpp>

#include <score/model/path/PathSerialization.hpp>
namespace Process
{

ChangePortSettings::ChangePortSettings(
    const Process::Port& p, Device::FullAddressAccessorSettings addr)
    : m_model{p}
    , m_old{p.settings()}
    , m_new{std::move(addr)}
{
}

void ChangePortSettings::undo(const score::DocumentContext& ctx) const
{
  auto& m = m_model.find(ctx);
  m.setSettings(m_old);
}

void ChangePortSettings::redo(const score::DocumentContext& ctx) const
{
  auto& m = m_model.find(ctx);
  m.setSettings(m_new);
}

void ChangePortSettings::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_old << m_new;
}

void ChangePortSettings::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_old >> m_new;
}
}
