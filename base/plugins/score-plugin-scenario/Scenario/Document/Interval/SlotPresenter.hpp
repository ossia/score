#pragma once

#include <Scenario/Document/Interval/SlotHeader.hpp>
#include <Scenario/Document/Interval/SlotHandle.hpp>
#include <Process/LayerPresenter.hpp>
#include <Scenario/Document/Interval/Temporal/DefaultHeaderDelegate.hpp>

namespace Scenario
{
struct LayerData;
class SlotHeader;
struct SlotPresenter
{
  SlotHeader* header{};
  Process::HeaderDelegate* headerDelegate{};
  SlotHandle* handle{};
  std::vector<LayerData> processes;
};
}
