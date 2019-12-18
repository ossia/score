#include "MidiOutletItem.hpp"
#include <Protocols/MIDI/MIDIProtocolFactory.hpp>
#include <Protocols/MIDI/MIDISpecificSettings.hpp>
#include <score/document/DocumentContext.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Process/Dataflow/AudioPortComboBox.hpp>
#include <Inspector/InspectorLayout.hpp>

namespace Dataflow
{
void MidiOutletFactory::setupOutletInspector(
        Process::Outlet &port,
        const score::DocumentContext &ctx,
        QWidget *parent,
        Inspector::Layout &lay,
        QObject *context)
{
  static const constexpr auto midi_uuid = Protocols::MIDIProtocolFactory::static_concreteKey();

    auto& p = static_cast<Process::MidiOutlet&>(port);

    auto& device = *ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
    QStringList midiDevices;
    midiDevices.push_back("");

    device.list().apply([&] (Device::DeviceInterface& dev) {
            auto& set = dev.settings();
            if(set.protocol == midi_uuid)
            {
                const auto& midi_set = set.deviceSpecificSettings.value<Protocols::MIDISpecificSettings>();
                if(midi_set.io == Protocols::MIDISpecificSettings::IO::In)
                  midiDevices.push_back(set.name);
            }
    });

    lay.addRow(Process::makeMidiCombo(midiDevices, port, ctx, parent));
}
}


#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/SetAutomationMax.hpp>
#include <Dataflow/Commands/CreateModulation.hpp>
#include <Dataflow/Commands/EditConnection.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <Device/Widgets/AddressAccessorEditWidget.hpp>
#include <Inspector/InspectorLayout.hpp>
#include <Process/Commands/EditPort.hpp>
#include <Process/Dataflow/AudioPortComboBox.hpp>
#include <Process/Dataflow/ControlWidgets.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortListWidget.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Process/Style/Pixmaps.hpp>
#include <Scenario/Commands/Interval/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <State/MessageListSerialization.hpp>
#include <Protocols/MIDI/MIDIProtocolFactory.hpp>
#include <Protocols/MIDI/MIDISpecificSettings.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/widgets/ControlWidgets.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <Device/ItemModels/NodeBasedItemModel.hpp>
#include <ossia/editor/state/destination_qualifiers.hpp>
#include <ossia/network/domain/domain.hpp>
#include <QDialog>
#include <QPainter>
#include <QDialogButtonBox>
#include <QGraphicsScene>
#include <QGraphicsSceneDragDropEvent>
#include <QMenu>
#include <QPushButton>
