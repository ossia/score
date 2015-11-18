#include "BaseStateMachine.hpp"

BaseStateMachine::BaseStateMachine(const QGraphicsScene &scene):
    m_scene{scene}
{

}

BaseStateMachine::~BaseStateMachine()
{

}

const QGraphicsScene &BaseStateMachine::scene() const
{
    return m_scene;
}

GraphicsSceneToolPalette::~GraphicsSceneToolPalette(){}
