// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Inspector/InspectorWidgetBase.hpp>
#include <Inspector/InspectorWidgetList.hpp>
#include <QPointer>
#include <QTabWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <boost/operators.hpp>
#include <score/widgets/MarginLess.hpp>

#include "InspectorPanel.hpp"
#include <Inspector/InspectorWidgetBase.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/model/IdentifiedObjectAbstract.hpp>

namespace score
{
class SelectionStack;
} // namespace score

namespace InspectorPanel
{
InspectorPanelWidget::InspectorPanelWidget(
    const Inspector::InspectorWidgetList& list,
    score::SelectionStack& s,
    QVBoxLayout* lay,
    QWidget* parent,
    QObject* parentObj)
    : QObject{parentObj}
    , m_parent{parent}
    , m_layout{lay}
    , m_list{list}
    , m_selectionDispatcher{s}
{
  m_layout->setContentsMargins(0, 0, 0, 0);
  m_layout->setSpacing(0);
}

void InspectorPanelWidget::newItemsInspected(const Selection& objects)
{
  QList<const IdentifiedObjectAbstract*> selectedObj;
  selectedObj.reserve(objects.size());
  for (auto& elt : objects)
  {
    if(elt)
      selectedObj.append(elt);
  }

  if (m_currentInspector)
  {
    m_layout->removeWidget(m_currentInspector);
    m_currentInspector->deleteLater();
    m_currentInspector = nullptr;
  }

  // All the objects selected ought to be in the same document.
  if (!selectedObj.empty())
  {
    auto& doc = score::IDocument::documentContext(*selectedObj.first());

    auto widgets = m_list.make(doc, selectedObj, m_parent);
    if(!widgets.empty())
    {
      m_layout->addWidget(widgets.first());
      m_currentInspector = widgets.first();
    }
    else
    {
      QString name = "Inspector";
      auto obj = selectedObj.first();
      if(auto meta = obj->findChild<score::ModelMetadata*>(QString{}, Qt::FindDirectChildrenOnly))
      {
        name = meta->getName();
      }
      m_currentInspector = new Inspector::InspectorWidgetBase{*selectedObj.first(), doc, m_parent, name};
      m_layout->addWidget(m_currentInspector);
    }
  }

  m_currentSel = objects.toList();
}

}
