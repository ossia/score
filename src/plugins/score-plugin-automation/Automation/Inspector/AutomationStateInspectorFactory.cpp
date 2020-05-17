// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AutomationStateInspectorFactory.hpp"

#include "AutomationStateInspector.hpp"

#include <Automation/State/AutomationState.hpp>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>

namespace Automation
{
StateInspectorFactory::StateInspectorFactory() : InspectorWidgetFactory{} { }

QWidget* StateInspectorFactory::make(
    const InspectedObjects& sourceElements,
    const score::DocumentContext& doc,
    QWidget* parent) const
{
  return new StateInspectorWidget{
      safe_cast<const ProcessState&>(*sourceElements.first()), doc, parent};
}

bool StateInspectorFactory::matches(const InspectedObjects& objects) const
{
  return dynamic_cast<const ProcessState*>(objects.first());
}
}
