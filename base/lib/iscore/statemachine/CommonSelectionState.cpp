#include "CommonSelectionState.hpp"
#include <QKeyEventTransition>
#include <QGraphicsObject>
#include <QFinalState>
#include "StateMachineUtils.hpp"

CommonSelectionState::CommonSelectionState(iscore::SelectionStack &stack, QGraphicsObject *process_view, QState *parent):
    QState{parent},
    dispatcher{stack}
{
    setChildMode(QState::ChildMode::ParallelStates);
    setObjectName("metaSelectionState");
    {
        // Multi-selection state
        auto selectionModeState = new QState{this};
        selectionModeState->setObjectName("selectionModeState");
        {
            m_singleSelection = new QState{selectionModeState};

            selectionModeState->setInitialState(m_singleSelection);
            m_multiSelection = new QState{selectionModeState};

            auto trans1 = new QKeyEventTransition(process_view,
                                                  QEvent::KeyPress, Qt::Key_Control, m_singleSelection);
            trans1->setTargetState(m_multiSelection);
            auto trans2 = new QKeyEventTransition(process_view,
                                                  QEvent::KeyRelease, Qt::Key_Control, m_multiSelection);
            trans2->setTargetState(m_singleSelection);
        }


        /// Proper selection stuff
        auto selectionState = new QState{this};
        selectionState->setObjectName("selectionState");
        {
            // Wait
            m_waitState = new QState{selectionState};
            m_waitState->setObjectName("m_waitState");
            selectionState->setInitialState(m_waitState);

            // Area
            auto selectionAreaState = new QState{selectionState};
            selectionAreaState->setObjectName("selectionAreaState");

            iscore::make_transition<iscore::Press_Transition>(m_waitState, selectionAreaState);
            selectionAreaState->addTransition(selectionAreaState, SIGNAL(finished()), m_waitState);
            {
                // States
                auto pressAreaSelection = new QState{selectionAreaState};
                pressAreaSelection->setObjectName("pressAreaSelection");
                selectionAreaState->setInitialState(pressAreaSelection);
                auto moveAreaSelection = new QState{selectionAreaState};
                moveAreaSelection->setObjectName("moveAreaSelection");
                auto releaseAreaSelection = new QFinalState{selectionAreaState};
                releaseAreaSelection->setObjectName("releaseAreaSelection");

                // Transitions
                iscore::make_transition<iscore::Move_Transition>(pressAreaSelection, moveAreaSelection);
                iscore::make_transition<iscore::Release_Transition>(pressAreaSelection, releaseAreaSelection);

                iscore::make_transition<iscore::Move_Transition>(moveAreaSelection, moveAreaSelection);
                iscore::make_transition<iscore::Release_Transition>(moveAreaSelection, releaseAreaSelection);

                // Operations
                connect(pressAreaSelection, &QState::entered,
                        this, &CommonSelectionState::on_pressAreaSelection);
                connect(moveAreaSelection, &QState::entered,
                        this, &CommonSelectionState::on_moveAreaSelection);
                connect(releaseAreaSelection, &QState::entered,
                        this, &CommonSelectionState::on_releaseAreaSelection);
            }

            // Deselection
            auto deselectState = new QState{selectionState};
            deselectState->setObjectName("deselectState");
            iscore::make_transition<iscore::Cancel_Transition>(selectionAreaState, deselectState);
            iscore::make_transition<iscore::Cancel_Transition>(m_waitState, deselectState);
            deselectState->addTransition(m_waitState);
            connect(deselectState, &QAbstractState::entered,
                    this, &CommonSelectionState::on_deselect);

            // Actions on selected elements
            auto t_delete = new QKeyEventTransition(
                                process_view, QEvent::KeyPress, Qt::Key_Delete, m_waitState);
            connect(t_delete, &QAbstractTransition::triggered,
                    this, &CommonSelectionState::on_delete);

            auto t_deleteContent = new QKeyEventTransition(
                                       process_view, QEvent::KeyPress, Qt::Key_Backspace, m_waitState);
            connect(t_deleteContent, &QAbstractTransition::triggered,
                    this, &CommonSelectionState::on_deleteContent);
        }
    }
}

CommonSelectionState::~CommonSelectionState()
{

}
