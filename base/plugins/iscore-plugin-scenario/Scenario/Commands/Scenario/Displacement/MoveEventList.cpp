#include <qiterator.h>
#include <stdexcept>
#include <algorithm>
#include "MoveEventList.hpp"
#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>

namespace Scenario
{
namespace Command
{
MoveEventFactoryInterface& MoveEventList::get(const iscore::ApplicationContext& ctx,
                                              MoveEventFactoryInterface::Strategy s) const
{
    auto it = std::max_element(begin(), end(), [&] (const auto& e1, const auto& e2)
    {
        return e1.priority(ctx, s) < e2.priority(ctx, s);
    });
    if(it != end())
        return *it;

    throw std::runtime_error("No moveEvent factories loaded");
}
}
}
