#pragma once
#include <Process/LayerModel.hpp>

namespace Space
{
class ProcessModel;
class LayerModel : public Process::LayerModel
{
        Q_OBJECT
    public:
        using model_type = Space::ProcessModel;
        LayerModel(
                const Id<Process::LayerModel>&,
                Space::ProcessModel&,
                QObject* parent);

        void serialize(const VisitorVariant &) const override;
        Process::LayerModelPanelProxy *make_panelProxy(QObject *parent) const override;
};
}
