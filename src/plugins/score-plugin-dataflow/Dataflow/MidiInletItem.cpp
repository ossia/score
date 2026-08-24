#include "MidiInletItem.hpp"

#include <Process/Dataflow/PortAddressComboBox.hpp>

#include <Inspector/InspectorLayout.hpp>

namespace Dataflow
{
void MidiInletFactory::setupInletInspector(
    const Process::Inlet& port, const score::DocumentContext& ctx, QWidget* parent,
    Inspector::Layout& lay, QObject* context)
{
  lay.addRow(port.name(), Process::makePortAddressCombo(port, ctx, parent));
}
}
