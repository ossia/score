#include "ToolState.hpp"
#include <iscore/statemachine/StateMachineUtils.hpp>
#include <QGraphicsScene>

ToolState::ToolState(const QGraphicsScene &scene, QState* parent):
    QState{parent},
    m_scene{scene}
{
    auto t_click = iscore::make_transition<iscore::Press_Transition>(this, this);
    connect(t_click, &QAbstractTransition::triggered,
            this,    &ToolState::on_pressed);

    auto t_move = iscore::make_transition<iscore::Move_Transition>(this, this);
    connect(t_move, &QAbstractTransition::triggered,
            this,   &ToolState::on_moved);

    auto t_rel = iscore::make_transition<iscore::Release_Transition>(this, this);
    connect(t_rel, &QAbstractTransition::triggered,
            this,  &ToolState::on_released);

    auto t_cancel = iscore::make_transition<iscore::Cancel_Transition>(this, this);
    connect(t_cancel, &QAbstractTransition::triggered,
            [&] () { localSM().postEvent(new iscore::Cancel_Event); });
}

QGraphicsItem* ToolState::itemUnderMouse(const QPointF &point) const
{
    return m_scene.itemAt(point, QTransform());
}

void ToolState::start()
{
    if(!localSM().isRunning())
        localSM().start();
}

void ToolState::stop()
{
    if(localSM().isRunning())
        localSM().stop();
}

ToolState::~ToolState()
{

}
