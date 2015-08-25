#include "ScenarioStateMachine.hpp"
#include "Tools/CreationToolState.hpp"
#include "Tools/SelectionToolState.hpp"
#include "Tools/MoveSlotToolState.hpp"

#include "Process/ScenarioModel.hpp"
#include "Process/Temporal/TemporalScenarioLayerModel.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"
#include "Process/Temporal/TemporalScenarioView.hpp"

#include <core/document/Document.hpp>
#include <iscore/document/DocumentInterface.hpp>

#include "Control/ScenarioControl.hpp"
#include <QSignalTransition>
#include <core/application/Application.hpp>

ScenarioStateMachine::ScenarioStateMachine(
        iscore::Document& doc,
        TemporalScenarioPresenter& presenter):
    BaseStateMachine{*presenter.view().scene()},
    m_presenter{presenter},
    m_commandStack{doc.commandStack()},
    m_locker{doc.locker()},
    m_expandMode{[] () -> auto&& {
        const auto& controls = iscore::Application::instance().presenter()->pluginControls();
        auto it = std::find_if(controls.begin(), controls.end(),
                            [] (iscore::PluginControlInterface* pc) { return qobject_cast<ScenarioControl*>(pc); });
        ISCORE_ASSERT(it != controls.end());
        return static_cast<ScenarioControl*>(*it)->expandMode();
    }()}
{
    this->setChildMode(ChildMode::ParallelStates);
    auto toolState = new QState{this};
    {
        createState = new CreationToolState{*this};
        createState->setParent(toolState);

        selectState = new SelectionTool{*this};
        selectState->setParent(toolState);
        toolState->setInitialState(selectState);

        moveSlotState = new MoveSlotToolState{*this};
        moveSlotState->setParent(toolState);

        transitionState = new QState{this};
        transitionState->setParent(toolState);


        auto QPointFToScenarioPoint = [&] (const QPointF& point) -> ScenarioPoint
        {
            return {TimeValue::fromMsecs(point.x() * m_presenter.zoomRatio()),
                    point.y() / m_presenter.view().boundingRect().height()};
        };

        connect(m_presenter.m_view, &TemporalScenarioView::scenarioPressed,
                [=] (const QPointF& point)
        {
            scenePoint = point;
            scenarioPoint = QPointFToScenarioPoint(m_presenter.m_view->mapFromScene(point));
            this->postEvent(new Press_Event);
        });
        connect(m_presenter.m_view, &TemporalScenarioView::scenarioReleased,
                [=] (const QPointF& point)
        {
            scenePoint = point;
            scenarioPoint = QPointFToScenarioPoint(m_presenter.m_view->mapFromScene(point));
            this->postEvent(new Release_Event);
        });
        connect(m_presenter.m_view, &TemporalScenarioView::scenarioMoved,
                [=] (const QPointF& point)
        {
            scenePoint = point;
            scenarioPoint = QPointFToScenarioPoint(m_presenter.m_view->mapFromScene(point));
            this->postEvent(new Move_Event);
        });
        connect(m_presenter.m_view, &TemporalScenarioView::escPressed,
                [=] () { this->postEvent(new Cancel_Event); });


        auto t_exit_select = new QSignalTransition(this, SIGNAL(exitState()), selectState);
        t_exit_select->setTargetState(transitionState);
        auto t_exit_moveSlot = new QSignalTransition(this, SIGNAL(exitState()), moveSlotState);
        t_exit_moveSlot->setTargetState(transitionState);
        auto t_exit_create = new QSignalTransition(this, SIGNAL(exitState()), createState);
        t_exit_create->setTargetState(transitionState);

        auto t_enter_select = new QSignalTransition(this, SIGNAL(setSelectState()), transitionState);
        t_enter_select->setTargetState(selectState);
        auto t_enter_moveSlot = new QSignalTransition(this, SIGNAL(setSlotMoveState()), transitionState);
        t_enter_moveSlot->setTargetState(moveSlotState);
        auto t_enter_create= new QSignalTransition(this, SIGNAL(setCreateState()), transitionState);
        t_enter_create->setTargetState(createState);

        createState->start();
        selectState->start();
        moveSlotState->start();
    }

    auto shiftModeState = new QState{this};
    {
        shiftReleasedState = new QState{shiftModeState};
        shiftModeState->setInitialState(shiftReleasedState);
        shiftPressedState = new QState{shiftModeState};

        auto t_shift_pressed = new QSignalTransition(this, SIGNAL(shiftPressed()), shiftReleasedState);
        t_shift_pressed->setTargetState(shiftPressedState);
        auto t_shift_released = new QSignalTransition(this, SIGNAL(shiftReleased()), shiftPressedState);
        t_shift_released->setTargetState(shiftReleasedState);
    }
}

const TemporalScenarioPresenter &ScenarioStateMachine::presenter() const
{
    return m_presenter;
}

const ScenarioModel& ScenarioStateMachine::model() const
{
    return static_cast<const ScenarioModel&>(m_presenter.m_layer.processModel());
}

Tool ScenarioStateMachine::tool() const
{
    if(createState->active())
        return Tool::Create;
    if(selectState->active())
        return Tool::Select;
    if(moveSlotState->active())
        return Tool::MoveSlot;

    return Tool::Select;
}

bool ScenarioStateMachine::isShiftPressed() const
{
    return shiftPressedState->active();
}


void ScenarioStateMachine::changeTool(int state)
{
    emit exitState();
    switch(state)
    {
    case static_cast<int>(Tool::Create):
        emit setCreateState();
        break;
    case static_cast<int>(Tool::MoveSlot):
        emit setSlotMoveState();
        break;
    case static_cast<int>(Tool::Select):
        emit setSelectState();
        break;

    default:
        ISCORE_ABORT;
        break;
    }
}
