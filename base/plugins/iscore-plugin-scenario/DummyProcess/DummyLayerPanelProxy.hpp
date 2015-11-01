#pragma once
#include <Process/LayerModelPanelProxy.hpp>

class DummyLayerPanelProxy final : public LayerModelPanelProxy
{
    public:
        explicit DummyLayerPanelProxy(
                const LayerModel& vm,
                QObject* parent);

        const LayerModel& layer() override;

    private:
        const LayerModel& m_layer;
};
