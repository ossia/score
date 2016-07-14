#pragma once
#include <Process/LayerModelPanelProxy.hpp>

#include <iscore_lib_process_export.h>

namespace Process { class LayerModel; }
class QObject;

namespace WidgetLayer
{
class ISCORE_LIB_PROCESS_EXPORT LayerPanelProxy final :
        public Process::LayerModelPanelProxy
{
    public:
        explicit LayerPanelProxy(
                const Process::LayerModel& vm,
                QObject* parent);


        const Process::LayerModel& layer() final override;
        QWidget* widget() const final override;

    private:
        const Process::LayerModel& m_layer;
        QWidget* m_widget{};
};
}
