#pragma once
#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>

#include <score_plugin_deviceexplorer_export.h>

class QMenu;
namespace score
{
struct DocumentContext;
}

namespace Explorer
{
class DeviceExplorerModel;

/**
 * Adds entries to the menu a right click opens in the device explorer: what a
 * plug-in can do with the devices and addresses selected there (record them,
 * ...), after the explorer's own entries.
 */
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT ExplorerMenuFactory : public score::InterfaceBase
{
  SCORE_INTERFACE(ExplorerMenuFactory, "c3a7e215-48d9-4b6f-9e02-7d1f5a8c4b96")
public:
  ~ExplorerMenuFactory() override;
  virtual void setupMenu(
      QMenu& menu, DeviceExplorerModel& explorer, const score::DocumentContext& ctx)
      = 0;
};

class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT ExplorerMenuFactoryList final
    : public score::InterfaceList<ExplorerMenuFactory>
{
public:
  ~ExplorerMenuFactoryList() override;
};
}
