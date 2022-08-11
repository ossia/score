#pragma once
#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>

#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>

namespace Scenario
{
namespace Command
{

class MoveEventList final : public score::InterfaceList<MoveEventFactoryInterface>
{
public:
  /**
   *@brief getMoveEventFactory
   *@return
   *the factory with the highest priority for the specified strategy
   */
  MoveEventFactoryInterface&
  get(const score::ApplicationContext& ctx, MoveEventFactoryInterface::Strategy s) const;
};
}
}
