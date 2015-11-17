#pragma once
#include <Process/LayerModelPanelProxy.hpp>
#include <Loop/LoopLayer.hpp>

class LoopPanelProxy final : public LayerModelPanelProxy
{
    public:
        LoopPanelProxy(
                const LoopLayer& lm,
                QObject* parent);

        const LoopLayer& layer() override;

    private:
        const LoopLayer& m_viewModel;
};
