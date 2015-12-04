#pragma once
#include <Process/LayerModelPanelProxy.hpp>

#include <iscore_lib_dummyprocess_export.h>

class LayerModel;
class QObject;

class ISCORE_LIB_DUMMYPROCESS_EXPORT DummyLayerPanelProxy final : public LayerModelPanelProxy
{
    public:
        explicit DummyLayerPanelProxy(
                const LayerModel& vm,
                QObject* parent);

        const LayerModel& layer() override;

    private:
        const LayerModel& m_layer;
};
