#pragma once
#include <Inspector/InspectorWidgetBase.hpp>
// TODO ProcessInspectorWidget

// TODO make factories, too.
// TODO Generic process factory like  Curve
struct DummyFactory
{
        using model = DummyModel;
        using layermodel = DummyLayerModel;
        using presenter = DummyLayerPresenter;
        using view = DummyLayerView;
};

class DummyInspectorWidget : public InspectorWidgetBase
{

};
