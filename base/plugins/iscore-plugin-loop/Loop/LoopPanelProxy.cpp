#include "LoopLayer.hpp"
#include "LoopPanelProxy.hpp"
#include <Process/LayerModelPanelProxy.hpp>

class QObject;

namespace Loop
{
PanelProxy::PanelProxy(
        const Layer& lm,
        QObject* parent):
    LayerModelPanelProxy{parent},
    m_viewModel{lm}
{

}

const Layer& PanelProxy::layer()
{
    return m_viewModel;
}
}
