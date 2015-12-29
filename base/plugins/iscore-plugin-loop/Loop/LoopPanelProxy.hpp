#pragma once
#include <Loop/LoopLayer.hpp>
#include <Process/LayerModelPanelProxy.hpp>

class QObject;

class LoopPanelProxy final : public Process::LayerModelPanelProxy
{
    public:
        LoopPanelProxy(
                const LoopLayer& lm,
                QObject* parent);

        const LoopLayer& layer() override;

    private:
        const LoopLayer& m_viewModel;
};
