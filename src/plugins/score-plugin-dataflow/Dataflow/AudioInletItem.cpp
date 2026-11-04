#include "AudioInletItem.hpp"

#include <Process/Dataflow/PortListWidget.hpp>

#include <Inspector/InspectorLayout.hpp>

namespace Dataflow
{
void AudioInletFactory::setupInletInspector(
    const Process::Inlet& port, const score::DocumentContext& ctx, QWidget* parent,
    Inspector::Layout& lay, QObject* context)
{
  Process::PortWidgetSetup::setupAddressPort(port, ctx, lay, parent);
}
}
