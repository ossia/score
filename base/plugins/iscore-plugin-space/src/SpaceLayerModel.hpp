#pragma once
#include <Process/LayerModel.hpp>

namespace Space
{
class ProcessModel;
class LayerModel : public ::LayerModel
{
        Q_OBJECT
    public:
        using model_type = Space::ProcessModel;
        LayerModel(
                const Id<::LayerModel>&,
                Space::ProcessModel&,
                QObject* parent);

        void serialize(const VisitorVariant &) const override;
        LayerModelPanelProxy *make_panelProxy(QObject *parent) const override;
};
}
