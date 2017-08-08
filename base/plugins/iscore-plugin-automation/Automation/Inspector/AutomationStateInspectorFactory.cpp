// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QString>

#include "AutomationStateInspector.hpp"
#include "AutomationStateInspectorFactory.hpp"
#include <Automation/State/AutomationState.hpp>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>

namespace Automation
{
StateInspectorFactory::StateInspectorFactory() : InspectorWidgetFactory{}
{
}

QWidget* StateInspectorFactory::make(
    const QList<const QObject*>& sourceElements,
    const iscore::DocumentContext& doc,
    QWidget* parent) const
{
  return new StateInspectorWidget{
      safe_cast<const ProcessState&>(*sourceElements.first()), doc, parent};
}

bool StateInspectorFactory::matches(const QList<const QObject*>& objects) const
{
  return dynamic_cast<const ProcessState*>(objects.first());
}
}
