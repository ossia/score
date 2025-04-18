#pragma once
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>

#include <score/command/AggregateCommand.hpp>

#include <score_plugin_deviceexplorer_export.h>
namespace Explorer
{
namespace Command
{
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT RemoveNodes final
    : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      DeviceExplorerCommandFactoryName(), RemoveNodes, "Remove Explorer nodes")
};
}
}
