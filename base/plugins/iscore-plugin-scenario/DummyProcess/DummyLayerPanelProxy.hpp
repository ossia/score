#pragma once
#include <Process/LayerModelPanelProxy.hpp>

#include <iscore_lib_dummyprocess_export.h>

namespace Process { class LayerModel; }
class QObject;

class ISCORE_LIB_DUMMYPROCESS_EXPORT DummyLayerPanelProxy final :
        public Process::LayerModelPanelProxy
{
    public:
        explicit DummyLayerPanelProxy(
                const Process::LayerModel& vm,
                QObject* parent);


        const Process::LayerModel& layer() final override;
        QWidget* widget() const final override;

    private:
        const Process::LayerModel& m_layer;
        QWidget* m_widget{};
};
