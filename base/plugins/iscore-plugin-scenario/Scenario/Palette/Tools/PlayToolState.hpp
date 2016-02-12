#pragma once


#include <QPoint>
#include <QStateMachine>

namespace Scenario
{
class ToolPalette;
struct Point;

class PlayToolState
{
    public:
    PlayToolState(const Scenario::ToolPalette &sm);

    void on_pressed(QPointF scenePoint, Scenario::Point scenarioPoint);
    void on_moved();
    void on_released();

    private:
    const Scenario::ToolPalette &m_sm;
};

}
