#include "CableInspector.hpp"

#include <Dataflow/Commands/EditConnection.hpp>
#include <score/widgets/SelectionButton.hpp>

#include <score/tools/Bind.hpp>
#include <score/widgets/SignalUtils.hpp>
#include <QVBoxLayout>
#include <wobjectimpl.h>
namespace Dataflow
{
static void fillPortName(QString& name, const Process::Port& port)
{
  if(auto p = port.parent())
  {
    auto model = p->findChild<score::ModelMetadata*>({}, Qt::FindDirectChildrenOnly);
    if(model)
    {
      if(!port.customData().isEmpty())
      {
        name += QString(": %1 (%2)")
            .arg(port.customData())
            .arg(model->getName());
      }
      else
      {
        name += QString(" (%1)")
            .arg(model->getName());
      }
    }
    else
    {
      if((p = p->parent()))
      {
         model = p->findChild<score::ModelMetadata*>({}, Qt::FindDirectChildrenOnly);
         if(model)
         {
           if(!port.customData().isEmpty())
           {
             name += QString(": %1 (%2)")
                 .arg(port.customData())
                 .arg(model->getName());
           }
           else
           {
             name += QString(" (%1)")
                 .arg(model->getName());
           }
         }
      }
    }
  }
}

CableWidget::CableWidget(
    const Process::Cable& cable,
    const score::DocumentContext& ctx,
    QWidget* parent)
    : InspectorWidgetBase{cable, ctx, parent, tr("Cable")}
    , m_selectionDispatcher{ctx.selectionStack}
{
  m_cableType.addItems(
      {tr("Immediate Glutton"),
       tr("Immediate Strict"),
       tr("Delayed Glutton"),
       tr("Delayed Strict")});
  m_cableType.setCurrentIndex((int)cable.type());

  con(m_cableType, SignalUtils::QComboBox_currentIndexChanged_int(),
      this, [&](int idx) {
    CommandDispatcher<> c{ctx.commandStack};
    c.submit<Dataflow::UpdateCable>(cable, (Process::CableType)idx);
  });

  this->updateAreaLayout({&m_cableType, &m_portList});

  auto lay = new QVBoxLayout{&m_portList};
  auto& source = cable.source().find(ctx);
  auto& sink = cable.sink().find(ctx);

  {
    QString name = tr("Source");
    fillPortName(name, source);
    auto b = SelectionButton::make(name, &source, m_selectionDispatcher, this);
    b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    lay->addWidget(b);
  }

  {
    QString name = tr("Sink");
    fillPortName(name, sink);
    auto b = SelectionButton::make(name, &sink, m_selectionDispatcher, this);
    b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    lay->addWidget(b);
  }
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
