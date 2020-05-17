#pragma once

#include "CSPCoherencyCheckerInterface.hpp"

#include <score/plugins/Interface.hpp>
#include <score/plugins/StringFactoryKey.hpp>

#include <score_plugin_scenario_export.h>
namespace score
{
struct ApplicationContext;
}
namespace Scenario
{
class SCORE_PLUGIN_SCENARIO_EXPORT CoherencyCheckerFactoryInterface : public score::InterfaceBase
{
  SCORE_INTERFACE(CoherencyCheckerFactoryInterface, "e9942ad6-1e39-4bdf-bb93-f31962e3cf79")

public:
  virtual CSPCoherencyCheckerInterface* make(
      Scenario::ProcessModel& scenario,
      const score::ApplicationContext& ctx,
      Scenario::ElementsProperties& elementsProperties)
      = 0;
  virtual ~CoherencyCheckerFactoryInterface();
};
}
