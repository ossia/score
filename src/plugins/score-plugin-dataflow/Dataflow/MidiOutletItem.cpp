#include "MidiOutletItem.hpp"

#include <Process/Dataflow/PortListWidget.hpp>

#include <Inspector/InspectorLayout.hpp>

namespace Dataflow
{
void MidiOutletFactory::setupOutletInspector(
    const Process::Outlet& port, const score::DocumentContext& ctx, QWidget* parent,
    Inspector::Layout& lay, QObject* context)
{
  Process::PortWidgetSetup::setupAddressPort(port, ctx, lay, parent);
}
}
