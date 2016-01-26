#pragma once
#include <Curve/Process/CurveProcessPanelProxy.hpp>

#include "MappingLayerModel.hpp"

namespace Mapping
{
class MappingPanelProxy : public Curve::CurveProcessPanelProxy<LayerModel>
{
    public:
        using CurveProcessPanelProxy<LayerModel>::CurveProcessPanelProxy;
};
}
