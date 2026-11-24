#pragma once
#include <Explorer/Explorer/ExplorerMenuFactory.hpp>

namespace Recording
{
//! The Record entries of the device explorer's menu: the devices and
//! addresses selected, into the document's main box, from now
class RecordExplorerMenu final : public Explorer::ExplorerMenuFactory
{
  SCORE_CONCRETE("f19b6c3e-2a48-4d75-8e01-6b3c9d2a7f54")
public:
  void setupMenu(
      QMenu& menu, Explorer::DeviceExplorerModel& explorer,
      const score::DocumentContext& ctx) override;
};
}
