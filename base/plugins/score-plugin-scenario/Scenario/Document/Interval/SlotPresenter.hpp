#pragma once

#include <Scenario/Document/Interval/SlotHeader.hpp>
#include <Scenario/Document/Interval/SlotHandle.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/HeaderDelegate.hpp>

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
    if(!header)
      return SlotHeader::headerHeight();

    if(!headerDelegate)
      return header->headerHeight();

    return headerDelegate->boundingRect().height();
  }
};
}
