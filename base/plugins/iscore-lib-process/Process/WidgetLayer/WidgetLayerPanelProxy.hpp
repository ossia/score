#pragma once
#include <Process/LayerModelPanelProxy.hpp>
#include <iscore/document/DocumentInterface.hpp>

#include <iscore_lib_process_export.h>

namespace Process { class LayerModel; }
class QObject;

namespace WidgetLayer
{
template<typename Process_T, typename Widget_T>
class LayerPanelProxy final :
        public Process::LayerModelPanelProxy
{
    public:
        explicit LayerPanelProxy(
                const Process::LayerModel& vm,
                QObject* parent):
            Process::LayerModelPanelProxy{parent},
            m_layer{vm}
        {
            m_widget = new Widget_T{
                    safe_cast<Process_T&>(vm.processModel()),
                    iscore::IDocument::documentContext(vm),
                    nullptr};
        }

        const Process::LayerModel& layer() final override
        {
            return m_layer;
        }

        QWidget* widget() const final override
        {
            return m_widget;
        }

    private:
        const Process::LayerModel& m_layer;
        QWidget* m_widget{};
};
}
