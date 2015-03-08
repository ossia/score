#pragma once

#include <QWidget>
#include <public_interface/selection/SelectionStack.hpp>
#include <public_interface/selection/SelectionDispatcher.hpp>
class SelectionStackWidget;
class QVBoxLayout;
class InspectorWidgetBase;
class QTabWidget;

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
        explicit InspectorPanel(iscore::SelectionStack& s, QWidget* parent);

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
        QTabWidget* m_tabWidget{};

        iscore::SelectionDispatcher* m_selectionDispatcher{};
};
