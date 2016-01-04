#include "DummyLayerPanelProxy.hpp"
#include <Process/LayerModelPanelProxy.hpp>

namespace Process { class LayerModel; }
class QObject;

DummyLayerPanelProxy::DummyLayerPanelProxy(
        const Process::LayerModel& vm,
        QObject* parent):
    Process::GraphicsViewLayerModelPanelProxy{vm, parent}
{

}
