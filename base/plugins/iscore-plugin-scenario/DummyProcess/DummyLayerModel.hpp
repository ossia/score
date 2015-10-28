#pragma once
#include <ProcessInterface/LayerModel.hpp>

class DummyLayerModel : public LayerModel
{
    public:
        void serialize(const VisitorVariant&) const;
        LayerModelPanelProxy*make_panelProxy(QObject* parent) const;
};
