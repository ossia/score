#include "MoveEventList.hpp"
#include "MoveEventFactoryInterface.hpp"

MoveEventFactoryInterface* MoveEventList::get(MoveEventList::Strategy strategy)
{
    QVectorIterator<MoveEventFactoryInterface*> factoriesIterator(m_list);

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

void MoveEventList::inscribe(MoveEventFactoryInterface* factoryInterface)
{
    MoveEventFactoryInterface* moveEventFactoryInterface = static_cast<MoveEventFactoryInterface*>(factoryInterface);

    m_list.push_back(moveEventFactoryInterface);
}

MoveEventList& SingletonMoveEventList::instance()
{
    static MoveEventList instance;
    return instance;
}
