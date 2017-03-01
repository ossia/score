#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <QList>
#include <QWidget>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity_fwd.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index_container.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>

#include <iscore/selection/Selection.hpp>

class IdentifiedObjectAbstract;
namespace Inspector
{
class InspectorWidgetList;
}

class QTabWidget;
class QVBoxLayout;
namespace iscore
{
class SelectionStack;
} // namespace iscore

namespace InspectorPanel
{
/*!
 * \brief The InspectorPanel class manages the main panel.
 *
 *  It creates and displays the view for each inspected element.
 */

// TODO rename file
class InspectorPanelWidget final : public QWidget
{
  Q_OBJECT

public:
  explicit InspectorPanelWidget(
      const Inspector::InspectorWidgetList& list,
      iscore::SelectionStack& s,
      QWidget* parent);

public slots:
  /*!
   * \brief newItemInspected load the view for the selected object
   *
   *  It's called when the user selects a new item
   * \param object The selected objet.
   */
  void newItemsInspected(const Selection&);

private:
  QVBoxLayout* m_layout{};
  QWidget* m_curWidget{};

  Inspector::InspectorWidgetBase* m_currentInspector{};

  const Inspector::InspectorWidgetList& m_list;
  iscore::SelectionDispatcher m_selectionDispatcher;
  QList<const IdentifiedObjectAbstract*> m_currentSel;
};
}
