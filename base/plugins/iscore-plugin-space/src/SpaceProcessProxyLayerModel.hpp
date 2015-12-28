#pragma once
#include <Process/LayerModel.hpp>
class SpaceProcessPanelProxy;
class SpaceLayerModel;
class SpaceProcessProxyLayerModel : public LayerModel
{
        Q_OBJECT
    public:
        SpaceProcessProxyLayerModel(
                const Id<LayerModel>&,
                const SpaceLayerModel& model,
                QObject* parent);

        void serialize(const VisitorVariant &) const override;
        LayerModelPanelProxy* make_panelProxy(QObject *parent) const override;

    private:
        const SpaceLayerModel& m_source;
};
