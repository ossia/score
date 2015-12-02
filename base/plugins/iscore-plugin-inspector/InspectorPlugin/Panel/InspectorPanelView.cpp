#include <Inspector/InspectorWidgetList.hpp>

#include <core/view/View.hpp>
#include <QBoxLayout>
#include <QLayout>
#include <qnamespace.h>
#include <QObject>
#include <QWidget>

#include "Implementation/InspectorPanel.hpp"
#include "Implementation/SelectionStackWidget.hpp"
#include "InspectorPanelView.hpp"
#include <iscore/application/ApplicationContext.hpp>
#include <core/document/Document.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/plugins/panel/PanelView.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/selection/SelectionStack.hpp>

static const iscore::DefaultPanelStatus status{true, Qt::RightDockWidgetArea, 10, QObject::tr("Inspector")};

const iscore::DefaultPanelStatus &InspectorPanelView::defaultPanelStatus() const
{ return status; }

InspectorPanelView::InspectorPanelView(iscore::View* parent) :
    iscore::PanelView {parent},
    m_widget{new QWidget}
{
    auto lay = new QVBoxLayout{m_widget};
    m_widget->setLayout(lay);
}

QWidget* InspectorPanelView::getWidget()
{
    return m_widget;
}

void InspectorPanelView::setCurrentDocument(iscore::Document* doc)
{
    using namespace iscore;

    delete m_stack;
    m_stack = nullptr;
    delete m_inspectorPanel;
    m_inspectorPanel = nullptr;

    m_currentDocument = doc;

    if(doc)
    {
        auto& appContext = doc->context().app;
        auto& fact = appContext.components.factory<InspectorWidgetList>();
        m_stack = new SelectionStackWidget{doc->selectionStack(), m_widget};
        m_inspectorPanel = new InspectorPanel{fact, doc->context().selectionStack, m_widget};

        m_widget->layout()->addWidget(m_stack);
        m_widget->layout()->addWidget(m_inspectorPanel);

        setNewSelection(doc->context().selectionStack.currentSelection());
    }
}

void InspectorPanelView::setNewSelection(const Selection& s)
{
    m_inspectorPanel->newItemsInspected(s);
}
