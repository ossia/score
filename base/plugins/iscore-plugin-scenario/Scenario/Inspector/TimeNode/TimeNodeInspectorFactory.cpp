// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QString>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>

#include "TimeNodeInspectorFactory.hpp"
#include "TimeNodeInspectorWidget.hpp"

namespace Scenario
{
Inspector::InspectorWidgetBase* TimeNodeInspectorFactory::makeWidget(
    const QList<const QObject*>& sourceElements,
    const iscore::DocumentContext& doc,
    QWidget* parent) const
{
  auto& timeNode = static_cast<const TimeNodeModel&>(*sourceElements.first());
  return new TimeNodeInspectorWidget{timeNode, doc, parent};
}

bool TimeNodeInspectorFactory::matches(
    const QList<const QObject*>& objects) const
{
  return dynamic_cast<const TimeNodeModel*>(objects.first());
}
}
