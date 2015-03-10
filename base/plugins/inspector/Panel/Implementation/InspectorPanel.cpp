#include "InspectorPanel.hpp"
#include "InspectorInterface/InspectorWidgetBase.hpp"
#include "InspectorInterface/InspectorSectionWidget.hpp"

#include "InspectorControl.hpp"

#include <iscore/selection/SelectionStack.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QTabWidget>

InspectorPanel::InspectorPanel(iscore::SelectionStack& s, QWidget* parent) :
    QWidget {parent},
    m_layout{new QVBoxLayout{this}},
    m_tabWidget{new QTabWidget{this}},
    m_selectionDispatcher{s}
{
    m_layout->setMargin(8);
    setMinimumWidth(250);
    m_layout->addWidget(m_tabWidget);
}

void InspectorPanel::newItemsInspected(const Selection& objects)
{
    delete m_tabWidget;

    m_tabWidget = new QTabWidget{this};
    m_layout->addWidget(m_tabWidget);

    m_tabWidget->setTabsClosable(true);
    for(auto object : objects)
    {
        auto widget = InspectorControl::makeInspectorWidget(object);
        m_tabWidget->addTab(widget, object->objectName());
    }

    connect(m_tabWidget,    &QTabWidget::tabCloseRequested,
            [=] (int index)
    {
        // need m_tabWidget.movable() = false !
        Selection sel = objects;
        sel.removeAt(index);
        m_selectionDispatcher.send(sel);
    });
}
