#include <Process/Commands/EditPort.hpp>
#include <Process/Dataflow/Port.hpp>

#include <score/model/path/PathSerialization.hpp>
namespace Process
{

ChangePortAddress::ChangePortAddress(const Process::Port& p, State::AddressAccessor addr)
    : m_model{p}, m_old{p.address()}, m_new{std::move(addr)}
{
}

void ChangePortAddress::undo(const score::DocumentContext& ctx) const
{
  auto& m = m_model.find(ctx);
  m.setAddress(m_old);
}

void ChangePortAddress::redo(const score::DocumentContext& ctx) const
{
  auto& m = m_model.find(ctx);
  m.setAddress(m_new);
}

void ChangePortAddress::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_old << m_new;
}

void ChangePortAddress::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_old >> m_new;
}
}
