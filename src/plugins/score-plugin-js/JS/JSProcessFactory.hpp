#pragma once
#include <JS/Inspector/JSInspectorWidget.hpp>
#include <JS/JSProcessMetadata.hpp>
#include <JS/JSProcessModel.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/WidgetLayer/WidgetProcessFactory.hpp>

namespace Process
{
template <>
inline JS::ProcessModel* ProcessFactory_T<JS::ProcessModel>::make(
    const TimeVal& duration,
    const QString& data,
    const Id<Process::ProcessModel>& id,
    const score::DocumentContext& ctx,
    QObject* parent)
{
  return new JS::ProcessModel{duration, data, id, parent};
}
}

namespace JS
{
using ProcessFactory = Process::ProcessFactory_T<JS::ProcessModel>;
using LayerFactory
    = WidgetLayer::LayerFactory<JS::ProcessModel, JS::InspectorWidget>;
}
