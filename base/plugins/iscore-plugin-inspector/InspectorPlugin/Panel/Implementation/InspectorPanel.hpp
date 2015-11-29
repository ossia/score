#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity_fwd.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index_container.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <qlist.h>
#include <qwidget.h>

#include "iscore/selection/Selection.hpp"

class IdentifiedObjectAbstract;
class InspectorWidgetList;
class QTabWidget;
class QVBoxLayout;
namespace boost {
namespace multi_index {
template <class Class, typename Type, Type (Class::*PtrToMemberFunction)() const> struct const_mem_fun;
}  // namespace multi_index
}  // namespace boost
namespace iscore {
class SelectionStack;
}  // namespace iscore

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
}
namespace Ui
{
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
