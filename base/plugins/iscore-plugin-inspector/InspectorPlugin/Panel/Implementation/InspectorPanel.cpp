#include <Inspector/InspectorWidgetList.hpp>
#include <boost/operators.hpp>
#include <QVBoxLayout>
#include <QPointer>
#include <QTabWidget>
#include <algorithm>

#include <Inspector/InspectorWidgetBase.hpp>
#include "InspectorPanel.hpp"
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/tools/IdentifiedObjectAbstract.hpp>
#include <iscore/document/DocumentInterface.hpp>

namespace iscore {
class SelectionStack;
}  // namespace iscore

namespace InspectorPanel
{
InspectorPanelWidget::InspectorPanelWidget(
        const Inspector::InspectorWidgetList& list,
        iscore::SelectionStack& s,
        QWidget* parent) :
    QWidget {parent},
    m_layout{new QVBoxLayout{this}},
    m_tabWidget{new QTabWidget{this}},
    m_list{list},
    m_selectionDispatcher{s}
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    setMinimumWidth(350);
    m_layout->addWidget(m_tabWidget);

    connect(m_tabWidget,    &QTabWidget::tabCloseRequested,
            this, &InspectorPanelWidget::on_tabClose);
}

void InspectorPanelWidget::newItemsInspected(const Selection& objects)
{
    QList<const IdentifiedObjectAbstract*> selectedObj;
    for(auto elt : objects)
    {
        selectedObj.append(elt);
    }

    if(m_currentInspector)
    {
        m_currentInspector->deleteLater();
        m_currentInspector = nullptr;
    }

    // All the objects selected ought to be in the same document.
    if(!objects.empty())
    {
        auto& doc = iscore::IDocument::documentContext(*selectedObj.first());

        auto widgets = m_list.make(
                    doc,
                    selectedObj,
                    m_tabWidget);

        m_tabWidget->addTab(widgets.first(), widgets.first()->tabName());
        m_currentInspector = widgets.first();
    }

    m_currentSel = objects.toList();
}

void InspectorPanelWidget::on_tabClose(int index)
{
    auto inspector_widget = static_cast<Inspector::InspectorWidgetBase*>(m_tabWidget->widget(index));
    // TODO need m_tabWidget.movable() = false !

    Selection sel = Selection::fromList(m_currentSel);
    sel.removeAll(inspector_widget->inspectedObject_addr());

    m_selectionDispatcher.setAndCommit(sel);
}
}
