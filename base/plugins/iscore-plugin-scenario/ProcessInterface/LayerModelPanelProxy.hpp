#pragma once
#include <QObject>
#include "LayerModel.hpp"
class LayerModelPanelProxy : public QObject
{
    public:
        using QObject::QObject;
        virtual ~LayerModelPanelProxy() = default;

        // Can return the same view model, or a new one.
        virtual const LayerModel& viewModel() = 0;

};
