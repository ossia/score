#pragma once
#include <ProcessInterface/LayerModel.hpp>
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

        void serialize(const VisitorVariant &) const;
        LayerModelPanelProxy* make_panelProxy(QObject *parent) const;

    private:
        const SpaceLayerModel& m_source;
};
