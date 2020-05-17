#include "CableInspector.hpp"

#include <Dataflow/Commands/EditConnection.hpp>

#include <score/tools/Bind.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <wobjectimpl.h>
namespace Dataflow
{

CableWidget::CableWidget(
    const Process::Cable& cable,
    const score::DocumentContext& ctx,
    QWidget* parent)
    : InspectorWidgetBase{cable, ctx, parent, tr("Cable")}
{
  m_cabletype.addItems(
      {tr("Immediate Glutton"),
       tr("Immediate Strict"),
       tr("Delayed Glutton"),
       tr("Delayed Strict")});
  m_cabletype.setCurrentIndex((int)cable.type());

  con(m_cabletype, SignalUtils::QComboBox_currentIndexChanged_int(), this, [&](int idx) {
    CommandDispatcher<> c{ctx.commandStack};
    c.submit<Dataflow::UpdateCable>(cable, (Process::CableType)idx);
  });

  this->updateAreaLayout({&m_cabletype});
}

CableInspectorFactory::CableInspectorFactory() : InspectorWidgetFactory{} { }

QWidget* CableInspectorFactory::make(
    const InspectedObjects& sourceElements,
    const score::DocumentContext& doc,
    QWidget* parent) const
{
  return new CableWidget{safe_cast<const Process::Cable&>(*sourceElements.first()), doc, parent};
}

bool CableInspectorFactory::matches(const InspectedObjects& objects) const
{
  return dynamic_cast<const Process::Cable*>(objects.first());
}
}
