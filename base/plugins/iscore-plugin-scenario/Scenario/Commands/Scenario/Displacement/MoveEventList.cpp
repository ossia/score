#include <qiterator.h>
#include <stdexcept>

#include "MoveEventList.hpp"
#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>

MoveEventFactoryInterface* MoveEventList::get(MoveEventFactoryInterface::Strategy strategy) const
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
