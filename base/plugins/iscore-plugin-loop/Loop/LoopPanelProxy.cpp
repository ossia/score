#include "LoopLayer.hpp"
#include "LoopPanelProxy.hpp"
#include <Process/LayerModelPanelProxy.hpp>

class QObject;

namespace Loop
{
PanelProxy::PanelProxy(
        const Layer& lm,
        QObject* parent):
    GraphicsViewLayerModelPanelProxy{lm, parent}
{

}
}
