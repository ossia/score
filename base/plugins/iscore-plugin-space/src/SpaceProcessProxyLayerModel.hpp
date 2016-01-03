#pragma once
#include <Process/LayerModel.hpp>
namespace Process { class LayerModel; }

namespace Space
{
class ProcessPanelProxy;
class ProcessProxyLayerModel : public Process::LayerModel
{
        Q_OBJECT
    public:
        ProcessProxyLayerModel(
                const Id<Process::LayerModel>&,
                const Process::LayerModel& model,
                QObject* parent);

        void serialize(const VisitorVariant &) const override;
        Process::LayerModelPanelProxy* make_panelProxy(QObject *parent) const override;

    private:
        const Process::LayerModel& m_source;
};
}
