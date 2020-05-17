#include "ControlOutletItem.hpp"

#include <Process/Dataflow/ControlWidgets.hpp>
#include <Process/Dataflow/PortListWidget.hpp>

#include <ossia/network/domain/domain.hpp>

namespace Dataflow
{

void ControlOutletFactory::setupOutletInspector(
    Process::Outlet& port,
    const score::DocumentContext& ctx,
    QWidget* parent,
    Inspector::Layout& lay,
    QObject* context)
{
  using namespace Process;
  auto& ctrl = static_cast<Process::ControlOutlet&>(port);
  PortWidgetSetup::setupInLayout(port, ctx, lay, parent);
}

}
