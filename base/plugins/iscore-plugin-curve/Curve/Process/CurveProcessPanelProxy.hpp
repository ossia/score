#pragma once
#include <Process/LayerModelPanelProxy.hpp>
#include <iscore_plugin_curve_export.h>

template<typename LayerModel_T>
class ISCORE_PLUGIN_CURVE_EXPORT CurveProcessPanelProxy :
        public Process::LayerModelPanelProxy
{
    public:
        CurveProcessPanelProxy(
                const LayerModel_T& vm,
                QObject* parent):
            LayerModelPanelProxy{parent},
            m_viewModel{vm}
        {

        }

        virtual ~CurveProcessPanelProxy() = default;

        const LayerModel_T& layer() override
        { return m_viewModel; }

    private:
        const LayerModel_T& m_viewModel;
};
