#pragma once
#include <JS/Inspector/JSInspectorWidget.hpp>
#include <JS/JSProcessMetadata.hpp>
#include <JS/JSProcessModel.hpp>
#include <Effect/EffectFactory.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/WidgetLayer/WidgetProcessFactory.hpp>
#include <Process/Script/ScriptEditor.hpp>
#include <Control/DefaultEffectItem.hpp>

namespace JS
{
using ProcessFactory = Process::ProcessFactory_T<JS::ProcessModel>;
using LayerFactory = Process::EffectLayerFactory_T<
    ProcessModel,
    Process::DefaultEffectItem,
    Process::ProcessScriptEditDialog<ProcessModel, ProcessModel::p_script>
>;
}
