#include "MoveEventList.hpp"
#include "MoveEventFactoryInterface.hpp"

MoveEventFactoryInterface*MoveEventList::getMoveEventFactory(MoveEventList::Strategy strategy)
{
    QVectorIterator<MoveEventFactoryInterface*> factoriesIterator(m_moveEventFactories);

    MoveEventFactoryInterface* bestFactory;

    if(factoriesIterator.hasNext())
    {
        bestFactory = factoriesIterator.next();
        while(factoriesIterator.hasNext())
        {
            MoveEventFactoryInterface* queriedFactory = factoriesIterator.next();
            if(queriedFactory->priority(strategy) > bestFactory->priority(strategy))
            {
                bestFactory = queriedFactory;
            }
        }
    }else
    {
        throw std::runtime_error("No moveEvent factories loaded");
    }

    return bestFactory;
}

void MoveEventList::registerMoveEventFactory(iscore::FactoryInterface* factoryInterface)
{
    MoveEventFactoryInterface* moveEventFactoryInterface = static_cast<MoveEventFactoryInterface*>(factoryInterface);

    m_moveEventFactories.push_back(moveEventFactoryInterface);
}
