#include "InspectorPanelView.hpp"
#include <core/view/View.hpp>
#include <core/document/Document.hpp>
#include <QVBoxLayout>

#include "Implementation/InspectorPanel.hpp"
#include "Implementation/SelectionStackWidget.hpp"

InspectorPanelView::InspectorPanelView(iscore::View* parent) :
    iscore::PanelViewInterface {parent},
    m_widget{new QWidget}
{
    this->setObjectName(tr("Inspector"));
    auto lay = new QVBoxLayout{m_widget};
    m_widget->setLayout(lay);
    m_widget->setMinimumWidth(350);
    m_widget->setMaximumWidth(350);
}

QWidget* InspectorPanelView::getWidget()
{
    return m_widget;
}

void InspectorPanelView::setCurrentDocument(iscore::Document* doc)
{
    using namespace iscore;

    m_widget->layout()->removeWidget(m_stack);
    m_widget->layout()->removeWidget(m_inspectorPanel);

    delete m_stack;
    delete m_inspectorPanel;

    m_currentDocument = doc;

    m_stack = new SelectionStackWidget{doc->selectionStack(), m_widget};
    m_inspectorPanel = new InspectorPanel{doc->selectionStack(), m_widget};

    m_widget->layout()->addWidget(m_stack);
    m_widget->layout()->addWidget(m_inspectorPanel);

    setNewSelection(doc->selectionStack().currentSelection());
}

void InspectorPanelView::setNewSelection(const Selection& s)
{
    m_inspectorPanel->newItemsInspected(s);
}
