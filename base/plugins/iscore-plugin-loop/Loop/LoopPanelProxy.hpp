#pragma once
#include <Loop/LoopLayer.hpp>
#include <Process/LayerModelPanelProxy.hpp>

class QObject;

namespace Loop
{
class PanelProxy final : public Process::LayerModelPanelProxy
{
    public:
        PanelProxy(
                const Layer& lm,
                QObject* parent);

        const Layer& layer() override;

    private:
        const Layer& m_viewModel;
};
}
