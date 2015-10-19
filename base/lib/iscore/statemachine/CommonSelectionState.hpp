#pragma once
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/statemachine/StateMachineTools.hpp>
class QGraphicsObject;

namespace iscore
{
    class SelectionStack;
}

/**
 * @brief The CommonSelectionState class
 *
 * A generic state to handle traditional rectangular selection in a
 * QGraphicsScene.
 */
class CommonSelectionState : public QState
{
    public:
        iscore::SelectionDispatcher dispatcher;

        virtual void on_pressAreaSelection() = 0;
        virtual void on_moveAreaSelection() = 0;
        virtual void on_releaseAreaSelection() = 0;
        virtual void on_deselect() = 0;
        virtual void on_delete() = 0;
        virtual void on_deleteContent() = 0;

        bool multiSelection() const
        {
            return isStateActive(m_multiSelection);
        }

    protected:
        CommonSelectionState(
                iscore::SelectionStack& stack,
                QGraphicsObject* process_view,
                QState* parent);

    private:
        QState* m_singleSelection{};
        QState* m_multiSelection{};
        QState* m_waitState{};
};
