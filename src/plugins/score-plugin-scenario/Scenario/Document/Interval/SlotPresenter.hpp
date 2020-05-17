#pragma once

#include <Process/HeaderDelegate.hpp>
#include <Process/LayerPresenter.hpp>
#include <Scenario/Document/Interval/SlotHeader.hpp>

namespace Scenario
{
class LayerData;
class SlotHeader;
struct SlotPresenter
{
  SlotHeader* header{};
  Process::HeaderDelegate* headerDelegate{};
  SlotFooter* footer{};
  Process::FooterDelegate* footerDelegate{};
  std::vector<LayerData> layers;

  void cleanupHeaderFooter();
  void cleanup(QGraphicsScene* sc);

  double headerHeight() const noexcept
  {
    if (!header)
      return SlotHeader::headerHeight();

    if (!headerDelegate)
      return header->headerHeight();

    return headerDelegate->boundingRect().height();
  }

  double footerHeight() const noexcept { return SlotFooter::footerHeight(); }
};

}
