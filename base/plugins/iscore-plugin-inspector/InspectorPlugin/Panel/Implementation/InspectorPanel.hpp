#pragma once

#include <QWidget>
#include <iscore/selection/SelectionStack.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <Inspector/InspectorWidgetBase.hpp>


class SelectionStackWidget;
class QVBoxLayout;
class InspectorWidgetBase;
class QTabWidget;
class InspectorWidgetList;

namespace bmi = boost::multi_index;
using InspectorWidgetMap = bmi::multi_index_container<
    InspectorWidgetBase*,
    bmi::indexed_by<
        bmi::hashed_unique<
            bmi::const_mem_fun<
                InspectorWidgetBase,
                const IdentifiedObjectAbstract*,
                &InspectorWidgetBase::inspectedObject_addr
            >
        >
    >
>;
namespace iscore
{
    class SerializableCommand;
}
namespace Ui
{
    class InspectorPanel;
}

/*!
 * \brief The InspectorPanel class manages the main panel.
 *
 *  It creates and displays the view for each inspected element.
 */


class InspectorPanel : public QWidget
{
        Q_OBJECT

    public:
        explicit InspectorPanel(
                const InspectorWidgetList& list,
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

        const InspectorWidgetList& m_list;
        iscore::SelectionDispatcher m_selectionDispatcher;
        QList<const IdentifiedObjectAbstract*> m_currentSel;
};
