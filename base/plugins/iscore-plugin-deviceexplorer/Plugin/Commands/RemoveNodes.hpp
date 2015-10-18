#pragma once

#include <iscore/command/AggregateCommand.hpp>

class RemoveNodes : public iscore::AggregateCommand
{
        ISCORE_AGGREGATE_COMMAND_DECL("DeviceExplorerControl", RemoveNodes, "RemoveNodes")
};
