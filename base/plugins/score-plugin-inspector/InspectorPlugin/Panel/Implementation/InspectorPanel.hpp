#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <wobjectdefs.h>
#include <QList>
#include <QWidget>
#include <multi_index/hashed_index.hpp>
#include <multi_index/identity_fwd.hpp>
#include <multi_index/indexed_by.hpp>
#include <multi_index/mem_fun.hpp>
#include <multi_index_container.hpp>
#include <score/selection/Selection.hpp>
#include <score/selection/SelectionDispatcher.hpp>

class IdentifiedObjectAbstract;
namespace Inspector
{
class InspectorWidgetList;
}

class QTabWidget;
class QVBoxLayout;
namespace score
{
class SelectionStack;
} // namespace score

namespace InspectorPanel
{
/*!
 * \brief The InspectorPanel class manages the main panel.
 *
 *  It creates and displays the view for each inspected element.
 */

// TODO rename file
class InspectorPanelWidget final : public QObject
{
  W_OBJECT(InspectorPanelWidget)

public:
  explicit InspectorPanelWidget(
      const Inspector::InspectorWidgetList& list,
      score::SelectionStack& s,
      QVBoxLayout* lay,
      QWidget* parent,
      QObject* parentObj);

public:
  /*!
   * \brief newItemInspected load the view for the selected object
   *
   *  It's called when the user selects a new item
   * \param object The selected objet.
   */
  void newItemsInspected(const Selection&); W_SLOT(newItemsInspected);

private:
  QWidget* m_parent{};
  QVBoxLayout* m_layout{};
  QWidget* m_curWidget{};

  QWidget* m_currentInspector{};

  const Inspector::InspectorWidgetList& m_list;
  score::SelectionDispatcher m_selectionDispatcher;
};
}
