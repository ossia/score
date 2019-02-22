#pragma once

#include <Process/HeaderDelegate.hpp>
#include <Process/LayerPresenter.hpp>
#include <Scenario/Document/Interval/SlotHandle.hpp>
#include <Scenario/Document/Interval/SlotHeader.hpp>

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

  double headerHeight() const
  {
    if (!header)
      return SlotHeader::headerHeight();

    if (!headerDelegate)
      return header->headerHeight();

    return headerDelegate->boundingRect().height();
  }
};
}
