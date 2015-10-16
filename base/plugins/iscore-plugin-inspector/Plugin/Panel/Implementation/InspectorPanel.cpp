#include "InspectorPanel.hpp"
#include <Inspector/InspectorSectionWidget.hpp>
#include <Inspector/InspectorWidgetList.hpp>

#include <QTabWidget>

InspectorPanel::InspectorPanel(iscore::SelectionStack& s, QWidget* parent) :
    QWidget {parent},
    m_layout{new QVBoxLayout{this}},
    m_tabWidget{new QTabWidget{this}},
    m_selectionDispatcher{s}
{
    m_tabWidget->setTabsClosable(true);
    m_layout->setContentsMargins(0, 0, 0, 0);
    setMinimumWidth(300);
    m_layout->addWidget(m_tabWidget);

    connect(m_tabWidget,    &QTabWidget::tabCloseRequested,
            this, &InspectorPanel::on_tabClose);
}

void InspectorPanel::newItemsInspected(const Selection& objects)
{
    // Ignore items in both
    // Create items in objects and not in current
    // Delete items in current and not in objects
    QList<const QObject*> toCreate, toDelete; // TODO not selection else with dead QPointers it will not work.

    // OPTIMIZEME (set_difference won't work due to unordered_set)
    for(auto& elt : objects)
    {
        auto it = std::find(m_currentSel.begin(), m_currentSel.end(), elt);
        if(it == m_currentSel.end())
            toCreate.append(elt);
    }

    for(auto& elt : m_currentSel)
    {
        auto it = std::find(objects.begin(), objects.end(), elt);
        if(it == objects.end())
            toDelete.append(elt);
    }

    for(const auto& object : toDelete)
    {
        auto& map =  m_map.get<0>();

        auto widget_it = map.find(object);
        if(widget_it != map.end())
        {
            auto ptr = *widget_it;
            map.erase(widget_it);
            ptr->deleteLater();
        }
    }

    for(const auto& object : toCreate)
    {
        auto widget = InspectorWidgetList::makeInspectorWidget(
                    object->objectName(),
                    *object,
                    m_tabWidget);
        m_tabWidget->addTab(widget, widget->tabName());
        m_map.insert(widget);
    }

    m_currentSel = objects.toList();
}

void InspectorPanel::on_tabClose(int index)
{
    auto inspector_widget = static_cast<InspectorWidgetBase*>(m_tabWidget->widget(index));
    // TODO need m_tabWidget.movable() = false !

    Selection sel = Selection::fromList(m_currentSel);
    sel.removeAll(inspector_widget->inspectedObject_addr());

    m_selectionDispatcher.setAndCommit(sel);
}
