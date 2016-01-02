#pragma once
#include <Process/LayerModel.hpp>
class SpaceProcessPanelProxy;
namespace Process { class LayerModel; }
class SpaceProcessProxyLayerModel : public Process::LayerModel
{
        Q_OBJECT
    public:
        SpaceProcessProxyLayerModel(
                const Id<Process::LayerModel>&,
                const Process::LayerModel& model,
                QObject* parent);

        void serialize(const VisitorVariant &) const override;
        Process::LayerModelPanelProxy* make_panelProxy(QObject *parent) const override;

    private:
        const Process::LayerModel& m_source;
};
