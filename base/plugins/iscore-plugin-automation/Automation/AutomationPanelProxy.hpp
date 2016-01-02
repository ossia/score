#pragma once
#include <Curve/Process/CurveProcessPanelProxy.hpp>

#include "AutomationLayerModel.hpp"

namespace Automation
{
class PanelProxy : public Curve::CurveProcessPanelProxy<LayerModel>
{
    public:
        using CurveProcessPanelProxy<LayerModel>::CurveProcessPanelProxy;
};
}
