#pragma once
#include <Automation/AutomationModel.hpp>
#include <Process/LayerModel.hpp>

namespace Automation
{
using Layer = Process::LayerModel_T<ProcessModel>;
}


LAYER_METADATA(
        ISCORE_PLUGIN_AUTOMATION_EXPORT,
        Automation::Layer,
        "657d8a05-6ee5-4093-8d61-d6ee2c425acf",
        "Automation",
        "Automation"
        )
