#include "InspectorPanelFactory.hpp"
#include "Implementation/InspectorPanel.hpp"
#include "Implementation/SelectionStackWidget.hpp"

#include <Inspector/InspectorWidgetList.hpp>

#include <iscore/selection/SelectionStack.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <QVBoxLayout>

namespace InspectorPanel
{
PanelDelegate::PanelDelegate(const iscore::ApplicationContext& ctx):
    iscore::PanelDelegate{ctx},
    m_widget{new QWidget}
{
    new iscore::MarginLess<QVBoxLayout>{m_widget};
}

QWidget* PanelDelegate::widget()
{
    return m_widget;
}

const iscore::PanelStatus&PanelDelegate::defaultPanelStatus() const
{
    static const iscore::PanelStatus status{
        true,
        Qt::RightDockWidgetArea,
                10,
                QObject::tr("Inspector"),
                QObject::tr("Ctrl+I")};

    return status;
}

std::unique_ptr<iscore::PanelDelegate> PanelDelegateFactory::make(
        const iscore::ApplicationContext& ctx)
{
    return std::make_unique<PanelDelegate>(ctx);
}


void PanelDelegate::on_modelChanged(
        iscore::PanelDelegate::maybe_document_t oldm,
        iscore::PanelDelegate::maybe_document_t newm)
{
    using namespace iscore;
    delete m_stack;
    m_stack = nullptr;
    delete m_inspectorPanel;
    m_inspectorPanel = nullptr;

    if(newm)
    {
        auto& fact = newm->app.components.factory<Inspector::InspectorWidgetList>();
        SelectionStack& stack = newm->selectionStack;
        m_stack = new SelectionStackWidget{stack, m_widget};
        m_inspectorPanel = new InspectorPanelWidget{fact, stack, m_widget};

        m_widget->layout()->addWidget(m_stack);
        m_widget->layout()->addWidget(m_inspectorPanel);

        setNewSelection(stack.currentSelection());
    }
}

void PanelDelegate::setNewSelection(const Selection& s)
{
    m_inspectorPanel->newItemsInspected(s);
}

}
