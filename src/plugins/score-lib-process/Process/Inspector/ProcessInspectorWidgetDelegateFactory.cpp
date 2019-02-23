// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ProcessInspectorWidgetDelegateFactory.hpp"

#include <Process/Dataflow/PortListWidget.hpp>
#include <Process/Process.hpp>

#include <score/widgets/TextLabel.hpp>

#include <QHBoxLayout>
#include <QTabWidget>
#include <QWidget>
namespace Process
{
InspectorWidgetDelegateFactory::~InspectorWidgetDelegateFactory() = default;

QWidget* InspectorWidgetDelegateFactory::make(
    const InspectedObjects& objects,
    const score::DocumentContext& doc,
    QWidget* parent) const
{
  if (objects.empty())
    return nullptr;

  auto obj = objects.first();
  if (auto p = qobject_cast<const Process::ProcessModel*>(obj))
  {
    return make_process(*p, doc, parent);
  }
  return nullptr;
}

bool InspectorWidgetDelegateFactory::matches(
    const InspectedObjects& objects) const
{
  if (objects.empty())
    return false;

  auto obj = objects.first();
  if (auto p = qobject_cast<const Process::ProcessModel*>(obj))
  {
    return matches_process(*p);
  }
  return false;
}

QWidget* InspectorWidgetDelegateFactory::wrap(
    const ProcessModel& process,
    const score::DocumentContext& doc,
    QWidget* w,
    QWidget* parent)
{
  auto widg = new QWidget{parent};
  auto lay = new QVBoxLayout{widg};

  auto label = new TextLabel{process.prettyShortName(), widg};

  label->setStyleSheet("font-weight: bold; font-size: 18");

  auto ports = new PortListWidget{process, doc, widg};
  lay->addWidget(label);
  auto tab = new QTabWidget;
  tab->setTabPosition(QTabWidget::South);
  tab->addTab(w, "Basic");
  tab->addTab(ports, "Ports");
  lay->addWidget(tab);

  return widg;
}
}
