// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ProcessInspectorWidgetDelegateFactory.hpp"
#include <Process/Process.hpp>
namespace Process
{
InspectorWidgetDelegateFactory::~InspectorWidgetDelegateFactory() = default;

QWidget*InspectorWidgetDelegateFactory::make(
    const QList<const QObject*>& objects,
    const score::DocumentContext& doc,
    QWidget* parent) const
{
  if(objects.empty())
    return nullptr;

  auto obj = objects.first();
  if(auto p = dynamic_cast<const Process::ProcessModel*>(obj))
  {
    return make_process(*p, doc, parent);
  }
  return nullptr;
}

bool InspectorWidgetDelegateFactory::matches(
    const QList<const QObject*>& objects) const
{
  if(objects.empty())
    return false;

  auto obj = objects.first();
  if(auto p = dynamic_cast<const Process::ProcessModel*>(obj))
  {
    return matches_process(*p);
  }
  return false;
}

}
