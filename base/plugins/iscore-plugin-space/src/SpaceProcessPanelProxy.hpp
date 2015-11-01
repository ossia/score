#pragma once
#include <Process/LayerModelPanelProxy.hpp>
#include "SpaceProcessProxyLayerModel.hpp"

class SpaceProcessProxyLayerModel;
class SpaceLayerModel;
class SpaceProcessPanelProxy : public LayerModelPanelProxy
{
    public:
        SpaceProcessPanelProxy(
                const SpaceLayerModel& vm,
                QObject* parent);

        const SpaceProcessProxyLayerModel& layer() override;

    private:
        SpaceProcessProxyLayerModel* m_layer{};

};
