#pragma once
#include <Process/LayerModelPanelProxy.hpp>
#include "SpaceProcessProxyLayerModel.hpp"

class SpaceProcessProxyLayerModel;
class LayerModel;
class SpaceProcessPanelProxy : public LayerModelPanelProxy
{
    public:
        SpaceProcessPanelProxy(
                const LayerModel& vm,
                QObject* parent);

        const SpaceProcessProxyLayerModel& layer() override;

    private:
        SpaceProcessProxyLayerModel* m_layer{};

};
