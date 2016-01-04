#pragma once
#include <Loop/LoopLayer.hpp>
#include <Process/LayerModelPanelProxy.hpp>

class QObject;

namespace Loop
{
class PanelProxy final : public Process::GraphicsViewLayerModelPanelProxy
{
    public:
        PanelProxy(
                const Layer& lm,
                QObject* parent);
};
}
