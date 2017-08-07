// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QString>
#include <Scenario/Document/Event/EventModel.hpp>

#include "EventInspectorFactory.hpp"
#include "EventInspectorWidget.hpp"

class QObject;
class QWidget;
namespace Scenario
{
QWidget* EventInspectorFactory::makeWidget(
    const QList<const QObject*>& sourceElements,
    const iscore::DocumentContext& doc,
    QWidget* parentWidget) const
{
  // TODO !!
  return new Inspector::InspectorWidgetBase{
      static_cast<const EventModel&>(*sourceElements.first()), doc,
      parentWidget};
}

bool EventInspectorFactory::matches(const QList<const QObject*>& objects) const
{
  return dynamic_cast<const EventModel*>(objects.first());
}
}
