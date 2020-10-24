#include "MidiInletItem.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Inspector/InspectorLayout.hpp>
#include <Process/Dataflow/AudioPortComboBox.hpp>
#include <Protocols/MIDI/MIDIProtocolFactory.hpp>
#include <Protocols/MIDI/MIDISpecificSettings.hpp>

#include <score/document/DocumentContext.hpp>

namespace Dataflow
{
void MidiInletFactory::setupInletInspector(
    const Process::Inlet& port,
    const score::DocumentContext& ctx,
    QWidget* parent,
    Inspector::Layout& lay,
    QObject* context)
{
  static const MSVC_BUGGY_CONSTEXPR auto midi_uuid
      = Protocols::MIDIInputProtocolFactory::static_concreteKey();

  auto& device = *ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
  QStringList midiDevices;
  midiDevices.push_back("");

  device.list().apply([&](Device::DeviceInterface& dev) {
    auto& set = dev.settings();
    if (set.protocol == midi_uuid)
    {
      const auto& midi_set = set.deviceSpecificSettings.value<Protocols::MIDISpecificSettings>();
      if (midi_set.io == Protocols::MIDISpecificSettings::IO::In)
        midiDevices.push_back(set.name);
    }
  });

  lay.addRow(Process::makeDeviceCombo(midiDevices, port, ctx, parent));
}

}
