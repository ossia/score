#pragma once
#include <Process/LayerModelPanelProxy.hpp>

template<typename LayerModel_T>
class CurveProcessPanelProxy : public LayerModelPanelProxy
{
    public:
        CurveProcessPanelProxy(
                const LayerModel_T& vm,
                QObject* parent):
            LayerModelPanelProxy{parent},
            m_viewModel{vm}
        {

        }

        const LayerModel_T& layer() override
        { return m_viewModel; }

    private:
        const LayerModel_T& m_viewModel;
};
