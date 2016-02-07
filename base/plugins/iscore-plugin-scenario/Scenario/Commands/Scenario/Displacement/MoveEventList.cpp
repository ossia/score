#include <qiterator.h>
#include <stdexcept>
#include <algorithm>
#include "MoveEventList.hpp"
#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>

namespace Scenario
{
namespace Command
{
MoveEventFactoryInterface& MoveEventList::get(MoveEventFactoryInterface::Strategy strategy) const
{
    auto it = std::max_element(begin(), end(), [&] (const auto& e1, const auto& e2)
    {
        return e1.priority(strategy) < e2.priority(strategy);
    });
    if(it != end())
        return *it;

    throw std::runtime_error("No moveEvent factories loaded");
}
}
}
