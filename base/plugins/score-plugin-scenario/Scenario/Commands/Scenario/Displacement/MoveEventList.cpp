// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MoveEventList.hpp"
#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>
#include <Scenario/Commands/Scenario/Displacement/SerializableMoveEvent.hpp>
#include <algorithm>
#include <qiterator.h>
#include <stdexcept>

namespace Scenario
{
namespace Command
{

SerializableMoveEvent::~SerializableMoveEvent()
{

}

MoveEventFactoryInterface& MoveEventList::get(
    const score::ApplicationContext& ctx,
    MoveEventFactoryInterface::Strategy s) const
{
  auto it
      = std::max_element(begin(), end(), [&](const auto& e1, const auto& e2) {
          return e1.priority(ctx, s) < e2.priority(ctx, s);
        });
  if (it != end())
    return *it;

  throw std::runtime_error("No moveEvent factories loaded");
}
}
}
