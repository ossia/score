#include "InspectorPanelView.hpp"
#include <core/view/View.hpp>
#include <core/document/DocumentPresenter.hpp>
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

void InspectorPanelView::setCurrentDocument(iscore::DocumentPresenter* pres)
{
    using namespace iscore;

    delete m_stack;
    delete m_inspectorPanel;
    m_stack = new SelectionStackWidget{&pres->selectionStack(), m_widget};
    m_inspectorPanel = new InspectorPanel{m_widget};

    // Selection
    connect(pres,             &DocumentPresenter::currentSelectionChanged,
            m_inspectorPanel, &InspectorPanel::newItemsInspected);

    connect(m_inspectorPanel, &InspectorPanel::selectedObjects,
            pres,             &DocumentPresenter::newSelection);

    // Commands
    connect(m_inspectorPanel, &InspectorPanel::submitCommand,
            pres,             &DocumentPresenter::applyCommand);


    auto lay = new QVBoxLayout{m_widget};
    lay->addWidget(m_stack);
    lay->addWidget(m_inspectorPanel);
}
