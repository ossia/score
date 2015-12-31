#pragma once
#include <Curve/Process/CurveProcessPanelProxy.hpp>

#include "AutomationLayerModel.hpp"
class AutomationPanelProxy : public Curve::CurveProcessPanelProxy<AutomationLayerModel>
{
    public:
        using CurveProcessPanelProxy<AutomationLayerModel>::CurveProcessPanelProxy;
};
