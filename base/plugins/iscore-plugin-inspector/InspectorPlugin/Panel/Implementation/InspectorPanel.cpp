#include <Inspector/InspectorWidgetList.hpp>
#include <QPointer>
#include <QTabWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <boost/operators.hpp>

#include "InspectorPanel.hpp"
#include <Inspector/InspectorWidgetBase.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/model/IdentifiedObjectAbstract.hpp>

namespace iscore
{
class SelectionStack;
} // namespace iscore

namespace InspectorPanel
{
InspectorPanelWidget::InspectorPanelWidget(
    const Inspector::InspectorWidgetList& list,
    iscore::SelectionStack& s,
    QWidget* parent)
    : QWidget{parent}
    , m_layout{new QVBoxLayout{this}}
    , m_list{list}
    , m_selectionDispatcher{s}
{
  m_layout->setContentsMargins(0, 0, 0, 0);
  m_layout->setSpacing(0);
  setMinimumWidth(350);
}

void InspectorPanelWidget::newItemsInspected(const Selection& objects)
{
  QList<const IdentifiedObjectAbstract*> selectedObj;
  selectedObj.reserve(objects.size());
  for (auto elt : objects)
  {
    if(elt)
      selectedObj.append(elt);
  }

  if (m_currentInspector)
  {
    m_currentInspector->deleteLater();
    m_currentInspector = nullptr;
  }

  // All the objects selected ought to be in the same document.
  if (!selectedObj.empty())
  {
    auto& doc = iscore::IDocument::documentContext(*selectedObj.first());

    auto widgets = m_list.make(doc, selectedObj, this);
    if(!widgets.empty())
    {
      m_layout->addWidget(widgets.first());
      m_currentInspector = widgets.first();
    }
  }

  m_currentSel = objects.toList();
}

}
