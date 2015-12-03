#include <qiterator.h>
#include <stdexcept>
#include <algorithm>
#include "MoveEventList.hpp"
#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>

MoveEventFactoryInterface* MoveEventList::get(MoveEventFactoryInterface::Strategy strategy) const
{
    auto it = std::max_element(m_list.begin(), m_list.end(), [&] (const auto& e1, const auto& e2)
    {
        return e1->priority(strategy) < e2->priority(strategy);
    });
    if(it != m_list.end())
        return it->get();

    throw std::runtime_error("No moveEvent factories loaded");
}
