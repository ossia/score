// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QString>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

#include "TimeSyncInspectorFactory.hpp"
#include "TimeSyncInspectorWidget.hpp"

namespace Scenario
{
QWidget* TimeSyncInspectorFactory::make(
    const QList<const QObject*>& sourceElements,
    const score::DocumentContext& doc,
    QWidget* parent) const
{
  auto& timeSync = static_cast<const TimeSyncModel&>(*sourceElements.first());
  return new TimeSyncInspectorWidget{timeSync, doc, parent};
}

bool TimeSyncInspectorFactory::matches(
    const QList<const QObject*>& objects) const
{
  return dynamic_cast<const TimeSyncModel*>(objects.first());
}
}
