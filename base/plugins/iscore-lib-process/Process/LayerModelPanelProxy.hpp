#pragma once
#include <qobject.h>

class LayerModel;

class LayerModelPanelProxy : public QObject
{
    public:
        using QObject::QObject;
        virtual ~LayerModelPanelProxy();

        // Can return the same view model, or a new one.
        virtual const LayerModel& layer() = 0;
};
