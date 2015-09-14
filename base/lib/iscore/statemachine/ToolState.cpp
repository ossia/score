#include "ToolState.hpp"
#include <iscore/statemachine/StateMachineUtils.hpp>
#include <QGraphicsScene>

ToolState::ToolState(const QGraphicsScene &scene, QState* parent):
    QState{parent},
    m_scene{scene}
{
    auto t_click = make_transition<Press_Transition>(this, this);
    connect(t_click, &QAbstractTransition::triggered,
            this,    &ToolState::on_pressed);

    auto t_move = make_transition<Move_Transition>(this, this);
    connect(t_move, &QAbstractTransition::triggered,
            this,   &ToolState::on_moved);

    auto t_rel = make_transition<Release_Transition>(this, this);
    connect(t_rel, &QAbstractTransition::triggered,
            this,  &ToolState::on_released);

    auto t_cancel = make_transition<Cancel_Transition>(this, this);
    connect(t_cancel, &QAbstractTransition::triggered,
            [&] () { localSM().postEvent(new Cancel_Event); });
}

QGraphicsItem* ToolState::itemUnderMouse(const QPointF &point) const
{
    return m_scene.itemAt(point, QTransform());
}

void ToolState::start()
{
    localSM().start();
}

void ToolState::stop()
{
    localSM().stop();
}
