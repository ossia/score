#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>

#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
namespace Scenario
{
namespace Command
{
class Macro;
}

class DropProcessOnState
{
public:
  bool drop(
      const Scenario::StateModel&, const Scenario::ProcessModel&, const QMimeData& mime,
      const score::DocumentContext& ctx);
};

}
