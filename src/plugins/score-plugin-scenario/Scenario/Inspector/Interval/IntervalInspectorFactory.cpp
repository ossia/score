// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "IntervalInspectorFactory.hpp"

#include "IntervalInspectorWidget.hpp"

#include <Inspector/InspectorWidgetList.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/plugins/StringFactoryKey.hpp>

class QObject;
class QWidget;

namespace Scenario
{
QWidget* IntervalInspectorFactory::make(
    const InspectedObjects& sourceElements,
    const score::DocumentContext& doc,
    QWidget* parent) const
{
  auto& appContext = doc.app;
  auto& widgetFact = appContext.interfaces<Inspector::InspectorWidgetList>();

  auto interval = dynamic_cast<const IntervalModel*>(sourceElements.first());
  if (interval)
    return new IntervalInspectorWidget{widgetFact, *interval, doc, parent};
  return nullptr;
}

bool IntervalInspectorFactory::matches(const InspectedObjects& objects) const
{
  return dynamic_cast<const IntervalModel*>(objects.first());
}
}
