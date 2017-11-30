// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Inspector/InspectorWidgetList.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Inspector/Interval/IntervalInspectorDelegateFactory.hpp>

#include <QString>
#include <score/application/ApplicationContext.hpp>

#include "IntervalInspectorFactory.hpp"
#include "IntervalInspectorWidget.hpp"
#include <Scenario/Inspector/Interval/IntervalInspectorDelegate.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/plugins/customfactory/StringFactoryKey.hpp>

class QObject;
class QWidget;

namespace Scenario
{
QWidget* IntervalInspectorFactory::make(
    const QList<const QObject*>& sourceElements,
    const score::DocumentContext& doc,
    QWidget* parent) const
{
  auto& appContext = doc.app;
  auto& widgetFact
      = appContext.interfaces<Inspector::InspectorWidgetList>();
  auto& intervalWidgetFactory
      = appContext
            .interfaces<IntervalInspectorDelegateFactoryList>();

  // make a relevant widget delegate :

  auto interval
      = dynamic_cast<const IntervalModel*>(sourceElements.first());

  if(interval)
    return new IntervalInspectorWidget{
      widgetFact, *interval,
          intervalWidgetFactory.make(
            &IntervalInspectorDelegateFactory::make, *interval),
          doc, parent};
  return nullptr;
}

bool IntervalInspectorFactory::matches(
    const QList<const QObject*>& objects) const
{
  return dynamic_cast<const IntervalModel*>(objects.first());
}
}
