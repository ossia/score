#pragma once
#include <Automation/AutomationModel.hpp>
#include <Process/LayerModel.hpp>

namespace Automation
{
using Layer = Process::LayerModel_T<ProcessModel>;
}

DEFAULT_MODEL_METADATA(Automation::Layer, "Automation layer")
