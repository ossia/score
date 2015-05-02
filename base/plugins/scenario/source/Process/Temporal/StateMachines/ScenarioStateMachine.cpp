#include "ScenarioStateMachine.hpp"
#include "Tools/CreationToolState.hpp"
#include "Tools/MoveToolState.hpp"
#include "Tools/SelectionToolState.hpp"
#include "Tools/MoveDeckToolState.hpp"

#include <QSignalTransition>

ScenarioStateMachine::ScenarioStateMachine(TemporalScenarioPresenter& presenter):
    m_presenter{presenter},
    m_commandStack{
        iscore::IDocument::documentFromObject(
            m_presenter.m_viewModel.sharedProcessModel())->commandStack()},
    m_locker{iscore::IDocument::documentFromObject(m_presenter.m_viewModel.sharedProcessModel())->locker()}
{
    this->setChildMode(ChildMode::ParallelStates);
    auto toolState = new QState{this};
    {
        createState = new CreationToolState{*this};
        createState->setParent(toolState);

        moveState = new MoveToolState{*this};
        moveState->setParent(toolState);

        selectState = new SelectionToolState{*this};
        selectState->setParent(toolState);
        toolState->setInitialState(selectState);

        moveDeckState = new MoveDeckToolState{*this};
        moveDeckState->setParent(toolState);


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

        // TODO how to avoid the combinatorial explosion ?
        auto t_move_create = new QSignalTransition(this, SIGNAL(setCreateState()), moveState);
        t_move_create->setTargetState(createState);
        auto t_move_select = new QSignalTransition(this, SIGNAL(setSelectState()), moveState);
        t_move_select->setTargetState(selectState);
        auto t_move_deckmove = new QSignalTransition(this, SIGNAL(setDeckMoveState()), moveState);
        t_move_deckmove->setTargetState(moveDeckState);

        auto t_select_create = new QSignalTransition(this, SIGNAL(setCreateState()), selectState);
        t_select_create->setTargetState(createState);
        auto t_select_move = new QSignalTransition(this, SIGNAL(setMoveState()), selectState);
        t_select_move->setTargetState(moveState);
        auto t_select_deckmove = new QSignalTransition(this, SIGNAL(setDeckMoveState()), selectState);
        t_select_deckmove->setTargetState(moveDeckState);

        auto t_create_move = new QSignalTransition(this, SIGNAL(setMoveState()), createState);
        t_create_move->setTargetState(moveState);
        auto t_create_select = new QSignalTransition(this, SIGNAL(setSelectState()), createState);
        t_create_select->setTargetState(selectState);
        auto t_create_deckmove = new QSignalTransition(this, SIGNAL(setDeckMoveState()), createState);
        t_create_deckmove->setTargetState(moveDeckState);

        auto t_movedeck_create = new QSignalTransition(this, SIGNAL(setCreateState()), moveDeckState);
        t_movedeck_create->setTargetState(createState);
        auto t_movedeck_select = new QSignalTransition(this, SIGNAL(setSelectState()), moveDeckState);
        t_movedeck_select->setTargetState(selectState);
        auto t_movedeck_move = new QSignalTransition(this, SIGNAL(setMoveState()), moveDeckState);
        t_movedeck_move->setTargetState(moveState);

        createState->start();
        moveState->start();
        selectState->start();
        moveDeckState->start();
    }

    auto expansionModeState = new QState{this};
    {
        scaleState = new QState{expansionModeState};
        expansionModeState->setInitialState(scaleState);
        growState = new QState{expansionModeState};

        auto t_scale_grow = new QSignalTransition(this, SIGNAL(setGrowState()), scaleState);
        t_scale_grow->setTargetState(growState);
        auto t_grow_scale = new QSignalTransition(this, SIGNAL(setScaleState()), growState);
        t_grow_scale->setTargetState(scaleState);
    }
}

const ScenarioModel& ScenarioStateMachine::model() const
{
    return static_cast<const ScenarioModel&>(m_presenter.m_viewModel.sharedProcessModel());
}

Tool ScenarioStateMachine::tool() const
{
    if(createState->active())
        return Tool::Create;
    if(selectState->active())
        return Tool::Select;
    if(moveState->active())
        return Tool::Move;
    if(moveDeckState->active())
        return Tool::MoveDeck;

    return Tool::Select;
}

ExpandMode ScenarioStateMachine::expandMode() const
{
    if(scaleState->active())
        return ExpandMode::Scale;
    if(growState->active())
        return ExpandMode::Grow;

    return ExpandMode::Scale;
}
