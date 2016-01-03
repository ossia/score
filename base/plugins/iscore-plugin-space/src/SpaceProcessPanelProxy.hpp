#pragma once
#include <Process/LayerModelPanelProxy.hpp>

namespace Process { class LayerModel; }
namespace Space
{
class ProcessProxyLayerModel;
class ProcessPanelProxy : public Process::LayerModelPanelProxy
{
    public:
        ProcessPanelProxy(
                const Process::LayerModel& vm,
                QObject* parent);

        const Process::LayerModel& layer() override;

    private:
        ProcessProxyLayerModel* m_layer{};

};
}
