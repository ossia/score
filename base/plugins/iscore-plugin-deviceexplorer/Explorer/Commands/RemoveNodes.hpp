#pragma once
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>

#include <iscore/command/AggregateCommand.hpp>

namespace Explorer
{
namespace Command
{
class RemoveNodes final : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), RemoveNodes, "Remove Explorer nodes")
};
}
}
