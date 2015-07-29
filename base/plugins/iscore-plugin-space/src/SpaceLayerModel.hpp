#pragma once
#include <ProcessInterface/LayerModel.hpp>

class SpaceProcess;
class SpaceLayerModel : public LayerModel
{
        Q_OBJECT
    public:
        SpaceLayerModel(
                const id_type<LayerModel>&,
                SpaceProcess&,
                QObject* parent);

        void serialize(const VisitorVariant &) const;
        LayerModelPanelProxy *make_panelProxy(QObject *parent) const;
};
