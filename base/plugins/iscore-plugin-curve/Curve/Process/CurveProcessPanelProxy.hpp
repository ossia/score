#pragma once
#include <Process/LayerModelPanelProxy.hpp>
#include <iscore_plugin_curve_export.h>

namespace Curve
{
template<typename LayerModel_T>
class CurveProcessPanelProxy :
        public Process::GraphicsViewLayerModelPanelProxy
{
    public:
        CurveProcessPanelProxy(
                const LayerModel_T& vm,
                QObject* parent):
            GraphicsViewLayerModelPanelProxy{vm, parent}
        {

        }

        virtual ~CurveProcessPanelProxy() = default;
};
}
