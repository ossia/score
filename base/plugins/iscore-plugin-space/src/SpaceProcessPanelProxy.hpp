#pragma once
#include <Process/LayerModelPanelProxy.hpp>
#include "SpaceProcessProxyLayerModel.hpp"

class SpaceProcessProxyLayerModel;
namespace Process { class LayerModel; }
class SpaceProcessPanelProxy : public Process::LayerModelPanelProxy
{
    public:
        SpaceProcessPanelProxy(
                const Process::LayerModel& vm,
                QObject* parent);

        const SpaceProcessProxyLayerModel& layer() override;

    private:
        SpaceProcessProxyLayerModel* m_layer{};

};
