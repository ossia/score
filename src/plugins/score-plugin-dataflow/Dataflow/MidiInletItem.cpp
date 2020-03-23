#include "MidiInletItem.hpp"
#include <Protocols/MIDI/MIDIProtocolFactory.hpp>
#include <Protocols/MIDI/MIDISpecificSettings.hpp>
#include <score/document/DocumentContext.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Process/Dataflow/AudioPortComboBox.hpp>
#include <Inspector/InspectorLayout.hpp>

namespace Dataflow
{
void MidiInletFactory::setupInletInspector(
        Process::Inlet &port,
        const score::DocumentContext &ctx,
        QWidget *parent,
        Inspector::Layout &lay,
        QObject *context)
{
  static const MSVC_BUGGY_CONSTEXPR auto midi_uuid = Protocols::MIDIProtocolFactory::static_concreteKey();

    auto& p = static_cast<Process::MidiInlet&>(port);

    auto& device = *ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
    QStringList midiDevices;
    midiDevices.push_back("");

    device.list().apply([&] (Device::DeviceInterface& dev) {
            auto& set = dev.settings();
            if(set.protocol == midi_uuid)
            {
                const auto& midi_set = set.deviceSpecificSettings.value<Protocols::MIDISpecificSettings>();
                if(midi_set.io == Protocols::MIDISpecificSettings::IO::Out)
                  midiDevices.push_back(set.name);
            }
    });

    lay.addRow(Process::makeMidiCombo(midiDevices, port, ctx, parent));
}

}
