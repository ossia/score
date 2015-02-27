#include "InspectorPanel.hpp"
#include "InspectorInterface/InspectorWidgetBase.hpp"
#include "InspectorInterface/InspectorSectionWidget.hpp"

#include "InspectorControl.hpp"

#include <core/interface/selection/SelectionStack.hpp>

#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QTabWidget>

InspectorPanel::InspectorPanel(QWidget* parent) :
    QWidget {parent},
    m_layout{new QVBoxLayout{this}},
    m_tabWidget{new QTabWidget{this}}
{
    m_layout->setMargin(8);
    setMinimumWidth(350);
    m_layout->addWidget(m_tabWidget);
}

void InspectorPanel::newItemsInspected(Selection objects)
{
    delete m_tabWidget;

    m_tabWidget = new QTabWidget{this};
    m_layout->addWidget(m_tabWidget);

    for(auto object : objects)
    {
        auto widget = InspectorControl::makeInspectorWidget(object);

        m_tabWidget->addTab(widget, object->objectName());

        connect(widget, SIGNAL(submitCommand(iscore::SerializableCommand*)),
                this,   SIGNAL(submitCommand(iscore::SerializableCommand*)));
        connect(widget, SIGNAL(objectsSelected(Selection)),
                this,   SIGNAL(newSelection(Selection)));

    }

    // TODO put this at the selection level instead.
    //connect(object, &QObject::destroyed,
    //        this,	&InspectorPanel::on_itemRemoved);
}
