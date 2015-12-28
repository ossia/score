#pragma once
#include <Process/LayerModel.hpp>

class SpaceProcess;
class SpaceLayerModel : public LayerModel
{
        Q_OBJECT
    public:
        using model_type = SpaceProcess;
        SpaceLayerModel(
                const Id<LayerModel>&,
                SpaceProcess&,
                QObject* parent);

        void serialize(const VisitorVariant &) const override;
        LayerModelPanelProxy *make_panelProxy(QObject *parent) const override;
};
