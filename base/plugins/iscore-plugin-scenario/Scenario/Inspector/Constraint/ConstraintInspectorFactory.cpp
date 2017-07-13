// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Inspector/InspectorWidgetList.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegateFactory.hpp>

#include <QString>
#include <iscore/application/ApplicationContext.hpp>

#include "ConstraintInspectorFactory.hpp"
#include "ConstraintInspectorWidget.hpp"
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegate.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>

class QObject;
class QWidget;

namespace Scenario
{
Inspector::InspectorWidgetBase* ConstraintInspectorFactory::makeWidget(
    const QList<const QObject*>& sourceElements,
    const iscore::DocumentContext& doc,
    QWidget* parent) const
{
  auto& appContext = doc.app;
  auto& widgetFact
      = appContext.interfaces<Inspector::InspectorWidgetList>();
  auto& processFact
      = appContext.interfaces<Process::ProcessFactoryList>();
  auto& constraintWidgetFactory
      = appContext
            .interfaces<ConstraintInspectorDelegateFactoryList>();

  // make a relevant widget delegate :

  auto& constraint
      = static_cast<const ConstraintModel&>(*sourceElements.first());

  return new ConstraintInspectorWidget{
      widgetFact, processFact, constraint,
      constraintWidgetFactory.make(
          &ConstraintInspectorDelegateFactory::make, constraint),
      doc, parent};
}

bool ConstraintInspectorFactory::matches(
    const QList<const QObject*>& objects) const
{
  return dynamic_cast<const ConstraintModel*>(objects.first());
}
}
