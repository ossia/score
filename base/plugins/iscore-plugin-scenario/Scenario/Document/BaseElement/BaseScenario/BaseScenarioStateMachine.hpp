#pragma once
#include <iscore/statemachine/BaseStateMachine.hpp>
#include <Scenario/Document/BaseElement/BaseScenario/StateMachine/BaseMoveSlot.hpp>
#include <Scenario/Application/ScenarioEditionSettings.hpp>



#include <Scenario/Commands/ResizeBaseConstraint.hpp>
#include <Scenario/Document/BaseElement/BaseScenario/BaseScenario.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/States/MoveStates.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/SelectionToolState.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/ConstraintTransitions.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/EventTransitions.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/StateTransitions.hpp>
#include <Scenario/Process/Temporal/StateMachines/Transitions/TimeNodeTransitions.hpp>
class MoveConstraintInBaseScenario_StateWrapper
{
    public:
        template<
                typename Scenario_T,
                typename ToolPalette_T>
        static void make(const ToolPalette_T& palette, QState* waitState, QStateMachine& sm)
        {
            // We cannot move the constraint
        }
};

class MoveEventInBaseScenario_StateWrapper
{
    public:
        template<
                typename Scenario_T,
                typename ToolPalette_T>
        static void make(const ToolPalette_T& palette, QState* waitState, QStateMachine& sm)
        {
            /// Event
            auto moveEvent =
                    new Scenario::MoveEventState<MoveBaseEvent, Scenario_T, ToolPalette_T>{
                        palette,
                        palette.model(),
                        palette.context().commandStack,
                        palette.context().objectLocker,
                        nullptr};

            make_transition<Scenario::ClickOnState_Transition<Scenario_T>>(waitState,
                                                     moveEvent,
                                                     *moveEvent);

            make_transition<Scenario::ClickOnEvent_Transition<Scenario_T>>(waitState,
                                                     moveEvent,
                                                     *moveEvent);
            moveEvent->addTransition(moveEvent,
                                       SIGNAL(finished()),
                                       waitState);
            sm.addState(moveEvent);
        }
};

class MoveTimeNodeInBaseScenario_StateWrapper
{
    public:
        template<
                typename Scenario_T,
                typename ToolPalette_T>
        static void make(const ToolPalette_T& palette, QState* waitState, QStateMachine& sm)
        {
            /// TimeNode
            auto moveTimeNode =
                    new Scenario::MoveTimeNodeState<MoveBaseEvent, Scenario_T, ToolPalette_T>{
                palette,
                        palette.model(),
                        palette.context().commandStack,
                        palette.context().objectLocker,
                        nullptr};

            make_transition<Scenario::ClickOnTimeNode_Transition<Scenario_T>>(waitState,
                                                                    moveTimeNode,
                                                                    *moveTimeNode);
            moveTimeNode->addTransition(moveTimeNode,
                                        SIGNAL(finished()),
                                        waitState);
            sm.addState(moveTimeNode);
        }
};

class DisplayedElementsPresenter;
class DisplayedElementsModel;
class BaseElementPresenter;
class ConstraintModel;

// TODO MoveMe to statemachine folder
// TODO rename me
class BaseScenario;
class BaseGraphicsObject;
class QGraphicsItem;

class BaseScenarioToolPalette final : public GraphicsSceneToolPalette
{
    public:
        BaseScenarioToolPalette(const BaseElementPresenter& pres);

        BaseGraphicsObject& view() const;
        const DisplayedElementsPresenter& presenter() const;
        const BaseScenario& model() const;
        const iscore::DocumentContext& context() const;
        const Scenario::EditionSettings& editionSettings() const;

    private:

        Scenario::Point ScenePointToScenarioPoint(QPointF point);


        const BaseElementPresenter& m_presenter;
        BaseMoveSlot m_slotTool;


        Scenario::SelectionAndMoveTool<
                BaseScenario,
                BaseScenarioToolPalette,
                BaseGraphicsObject,
                MoveConstraintInBaseScenario_StateWrapper,
                MoveEventInBaseScenario_StateWrapper,
                MoveTimeNodeInBaseScenario_StateWrapper
                >  m_state;
};

