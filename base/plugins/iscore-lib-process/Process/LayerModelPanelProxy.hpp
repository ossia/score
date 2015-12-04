#pragma once
#include <QObject>
#include <iscore_lib_process_export.h>

class LayerModel;

class ISCORE_LIB_PROCESS_EXPORT LayerModelPanelProxy : public QObject
{
    public:
        using QObject::QObject;
        virtual ~LayerModelPanelProxy();

        // Can return the same view model, or a new one.
        virtual const LayerModel& layer() = 0;
};
