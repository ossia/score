#include "ControlOutletItem.hpp"

#include <Process/Dataflow/ControlWidgets.hpp>
#include <Process/Dataflow/PortListWidget.hpp>

#include <ossia/network/domain/domain.hpp>

namespace Dataflow
{

void ControlOutletFactory::setupOutletInspector(
    const Process::Outlet& port,
    const score::DocumentContext& ctx,
    QWidget* parent,
    Inspector::Layout& lay,
    QObject* context)
{
  using namespace Process;
  PortWidgetSetup::setupInLayout(port, ctx, lay, parent);
}

}
