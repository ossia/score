#include "LoopPanelProxy.hpp"
#include "LoopLayer.hpp"

LoopPanelProxy::LoopPanelProxy(
        const LoopLayer& lm,
        QObject* parent):
    LayerModelPanelProxy{parent},
    m_viewModel{lm}
{

}

const LoopLayer& LoopPanelProxy::layer()
{
    return m_viewModel;
}
