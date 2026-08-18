#include "MidiInletItem.hpp"

#include <Process/Dataflow/AudioPortComboBox.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Inspector/InspectorLayout.hpp>

#if defined(SCORE_PLUGIN_PROTOCOLS)
#include <Protocols/MIDI/MIDIProtocolFactory.hpp>
#include <Protocols/MIDI/MIDISpecificSettings.hpp>
#endif

#include <score/document/DocumentContext.hpp>

namespace Dataflow
{
void MidiInletFactory::setupInletInspector(
    const Process::Inlet& port, const score::DocumentContext& ctx, QWidget* parent,
    Inspector::Layout& lay, QObject* context)
{
#if defined(SCORE_PLUGIN_PROTOCOLS)
  static const constexpr auto midi_uuid
      = Protocols::MIDIInputProtocolFactory::static_concreteKey();

  auto& device = *ctx.findPlugin<Explorer::DeviceDocumentPlugin>();



  lay.addRow(
      port.name(), Process::makeDeviceCombo(
                       Device::DeviceKind::MidiIn, device.list(), port, ctx, parent));
#endif
}

}
