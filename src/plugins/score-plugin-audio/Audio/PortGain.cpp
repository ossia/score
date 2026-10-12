#include "PortGain.hpp"

#include <Device/Node/DeviceNode.hpp>

#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Audio/AudioDevice.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

namespace Audio
{
const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"Audio"};
  return key;
}

SetPortGain::SetPortGain(State::Address address, double before, double after)
    : m_address{std::move(address)}
    , m_before{before}
    , m_after{after}
{
}

void SetPortGain::undo(const score::DocumentContext& ctx) const
{
  apply(ctx, m_before);
}

void SetPortGain::redo(const score::DocumentContext& ctx) const
{
  apply(ctx, m_after);
}

void SetPortGain::apply(const score::DocumentContext& ctx, double gain) const
{
  auto& plug = ctx.plugin<Explorer::DeviceDocumentPlugin>();
  if(auto node = Device::try_getNodeFromAddress(plug.rootNode(), m_address);
     node && node->is<Device::AddressSettings>())
    node->get<Device::AddressSettings>().extendedAttributes["audio-gain"] = gain;

  if(auto dev = static_cast<Dataflow::AudioDevice*>(plug.list().audioDevice()))
    dev->setGain(m_address, gain);
}

void SetPortGain::serializeImpl(DataStreamInput& s) const
{
  s << m_address << m_before << m_after;
}

void SetPortGain::deserializeImpl(DataStreamOutput& s)
{
  s >> m_address >> m_before >> m_after;
}

void commitPortGain(
    const score::DocumentContext& ctx, const State::Address& address, double before,
    double after)
{
  if(before == after)
    return;
  auto& plug = ctx.plugin<Explorer::DeviceDocumentPlugin>();
  auto dev = plug.list().audioDevice();
  if(!dev)
    return;

  const bool shown = ossia::any_of(plug.rootNode(), [&](const Device::Node& n) {
    return n.is<Device::DeviceSettings>()
           && n.get<Device::DeviceSettings>().name == dev->settings().name;
  });
  if(shown)
  {
    CommandDispatcher<>{ctx.commandStack}.submit(
        new SetPortGain{address, before, after});
    return;
  }

  RedoMacroCommandDispatcher<SetPortGainMacro> disp{ctx.commandStack};
  disp.submit(new Explorer::Command::LoadDevice{plug, dev->settings()});
  disp.submit(new SetPortGain{address, before, after});
  disp.commit();
}
}
