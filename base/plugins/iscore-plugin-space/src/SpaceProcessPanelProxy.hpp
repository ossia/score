#pragma once
#include <Process/LayerModelPanelProxy.hpp>

namespace Process { class LayerModel; }
namespace Space
{
class ProcessProxyLayerModel;
class ProcessPanelProxy : public Process::GraphicsViewLayerModelPanelProxy
{
    public:
        ProcessPanelProxy(
                ProcessProxyLayerModel* vm,
                QObject* parent);

    private:
        ProcessProxyLayerModel* m_layerImpl{};

};
}
