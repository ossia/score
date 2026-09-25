#include <Process/Commands/EditPort.hpp>
#include <Process/Dataflow/Port.hpp>

#include <LocalTree/ScriptableReference.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/path/PathSerialization.hpp>

#include <core/document/Document.hpp>
namespace Process
{

static State::AddressAccessor settled(const Port& port, State::AddressAccessor address)
{
  if(auto doc = score::IDocument::try_documentFromObject(port))
    LocalTree::settle(address, doc->context());
  return address;
}

ChangePortSettings::ChangePortSettings(
    const Process::Port& p, Device::FullAddressAccessorSettings addr)
    : m_model{p}
    , m_old{p.settings()}
    , m_new{std::move(addr)}
{
  m_new.address = settled(p, std::move(m_new.address));
}

ChangePortAddress::ChangePortAddress(const Port& port, State::AddressAccessor address)
    : PropertyCommand_T{port, settled(port, std::move(address))}
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
