#include "InspectorPanelView.hpp"
#include <core/view/View.hpp>
#include <core/document/Document.hpp>
#include <QVBoxLayout>

#include "Implementation/InspectorPanel.hpp"
#include <Panel/Implementation/SelectionStackWidget.hpp>

InspectorPanelView::InspectorPanelView(iscore::View* parent) :
    iscore::PanelViewInterface {parent},
    m_widget{new QWidget}
{
    this->setObjectName(tr("Inspector"));
}

QWidget* InspectorPanelView::getWidget()
{
    return m_widget;
}

void InspectorPanelView::setCurrentDocument(iscore::Document* doc)
{
    using namespace iscore;

    delete m_stack;
    delete m_inspectorPanel;

    m_currentDocument = doc;
    m_stack = new SelectionStackWidget{&doc->selectionStack(), m_widget};
    m_inspectorPanel = new InspectorPanel{doc->selectionStack(), m_widget};

    auto lay = new QVBoxLayout{m_widget};
    lay->addWidget(m_stack);
    lay->addWidget(m_inspectorPanel);
}

void InspectorPanelView::setNewSelection(const Selection& s)
{
    m_inspectorPanel->newItemsInspected(s);
}
