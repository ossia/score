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
    m_layout{new QVBoxLayout{this}}
{
    m_layout->setMargin(8);
    setMinimumWidth(300);
}

void InspectorPanel::newItemsInspected(Selection objects)
{
    delete m_tabWidget;

    m_tabWidget = new QTabWidget{this};
    for(auto object : objects)
    {
        auto widget = InspectorControl::makeInspectorWidget(object);

        m_tabWidget->addTab(widget, object->objectName());

        connect(widget, SIGNAL(submitCommand(iscore::SerializableCommand*)),
                this,   SIGNAL(submitCommand(iscore::SerializableCommand*)));
        connect(widget, SIGNAL(selectedObjects(Selection)),
                this,   SIGNAL(selectedObjects(Selection)));

    }

    m_layout->addWidget(m_tabWidget);
    // TODO put this at the selection level instead.
    //connect(object, &QObject::destroyed,
    //        this,	&InspectorPanel::on_itemRemoved);
}
