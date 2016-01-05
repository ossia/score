#include "Autom3DPanelProxy.hpp"
#include <Autom3D/Panel/AutomWidget.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <Autom3D/Autom3DModel.hpp>
namespace Autom3D
{
PanelProxy::PanelProxy(
        const Process::LayerModel& vm,
        QObject* parent):
    Process::LayerModelPanelProxy{parent},
    m_layer{vm}
{
    m_widget = new AutomWidget{
            static_cast<const ProcessModel&>(vm.processModel()),
            iscore::IDocument::documentContext(vm).commandStack};
}

const Process::LayerModel& PanelProxy::layer()
{
    return m_layer;
}

QWidget* PanelProxy::widget() const
{
    return m_widget;
}

}
