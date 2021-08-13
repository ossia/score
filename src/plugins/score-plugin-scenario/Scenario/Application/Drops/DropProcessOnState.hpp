#pragma once
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
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
      const Scenario::StateModel&,
      const Scenario::ProcessModel&,
      const QMimeData& mime,
      const score::DocumentContext& ctx);
};

}
