#include "LoopLayer.hpp"
#include "LoopPanelProxy.hpp"
#include <Process/LayerModelPanelProxy.hpp>

class QObject;

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
