#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity_fwd.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <QList>
#include <QWidget>

#include <iscore/selection/Selection.hpp>

class IdentifiedObjectAbstract;
namespace Inspector
{
class InspectorWidgetList;
}

class QTabWidget;
class QVBoxLayout;
namespace iscore {
class SelectionStack;
}  // namespace iscore

namespace InspectorPanel
{
namespace bmi = boost::multi_index;
using InspectorWidgetMap = bmi::multi_index_container<
    Inspector::InspectorWidgetBase*,
    bmi::indexed_by<
        bmi::hashed_unique<
            bmi::const_mem_fun<
                Inspector::InspectorWidgetBase,
                const IdentifiedObjectAbstract*,
                &Inspector::InspectorWidgetBase::inspectedObject_addr
            >
        >
    >
>;

/*!
 * \brief The InspectorPanel class manages the main panel.
 *
 *  It creates and displays the view for each inspected element.
 */

// TODO rename file
class InspectorPanelWidget : public QWidget
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

        void on_tabClose(int index);

    private:
        QVBoxLayout* m_layout{};
        QTabWidget* m_tabWidget{};
        InspectorWidgetMap m_map;

        const Inspector::InspectorWidgetList& m_list;
        iscore::SelectionDispatcher m_selectionDispatcher;
        QList<const IdentifiedObjectAbstract*> m_currentSel;
};
}
