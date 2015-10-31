#pragma once
#include <Curve/Process/CurveProcessPanelProxy.hpp>

#include "AutomationLayerModel.hpp"
class AutomationPanelProxy : public CurveProcessPanelProxy<AutomationLayerModel>
{
    public:
        using CurveProcessPanelProxy<AutomationLayerModel>::CurveProcessPanelProxy;
};
