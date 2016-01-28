#pragma once
#include <Process/LayerModelPanelProxy.hpp>
#include <iscore_plugin_curve_export.h>

namespace Curve
{
template<typename LayerModel_T>
class ISCORE_PLUGIN_CURVE_EXPORT CurveProcessPanelProxy :
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
