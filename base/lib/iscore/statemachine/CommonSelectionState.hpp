#pragma once
#include <QState>
#include <iscore/selection/SelectionDispatcher.hpp>
class QGraphicsObject;

namespace iscore
{
    class SelectionStack;
}
class CommonSelectionState : public QState
{
    private:
        QState* m_singleSelection{};
        QState* m_multiSelection{};
        QState* m_waitState{};


    public:
        iscore::SelectionDispatcher dispatcher;

        CommonSelectionState(
                iscore::SelectionStack& stack,
                QGraphicsObject* process_view,
                QState* parent);

        virtual void on_pressAreaSelection() = 0;
        virtual void on_moveAreaSelection() = 0;
        virtual void on_releaseAreaSelection() = 0;
        virtual void on_deselect() = 0;
        virtual void on_delete() = 0;
        virtual void on_deleteContent() = 0;

        bool multiSelection() const
        {
            return m_multiSelection->active();
        }
};
