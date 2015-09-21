#include "MoveEventList.hpp"

MoveEventFactoryInterface*
MoveEventList::getMoveEventFactory()
{
    return m_moveEventFactories.last();
}

void
MoveEventList::registerMoveEventFactory(iscore::FactoryInterface* factoryInterface)
{
    MoveEventFactoryInterface* moveEventFactoryInterface = static_cast<MoveEventFactoryInterface*>(factoryInterface);

    m_moveEventFactories.insert(moveEventFactoryInterface->priority(), moveEventFactoryInterface);
}
