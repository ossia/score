#pragma once

#include <QWidget>
#include <core/interface/selection/SelectionStack.hpp>
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
        explicit InspectorPanel(QWidget* parent);

    signals:
        void submitCommand(iscore::SerializableCommand*);

        /**
         * @brief selectedObjects
         * This signal is sent when objects are selected FROM the inspector.
         */
        void selectedObjects(Selection);

    public slots:
        /*!
         * \brief newItemInspected load the view for the selected object
         *
         *  It's called when the user selects a new item
         * \param object The selected objet.
         */
        void newItemsInspected(Selection);

    private:
        QVBoxLayout* m_layout{};

        QTabWidget* m_tabWidget{};
};
