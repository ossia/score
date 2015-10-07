#pragma once

#include <iscore/command/AggregateCommand.hpp>

class RemoveNodes : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL("DeviceExplorerControl", "RemoveNodes", "RemoveNodes")
         public:
             RemoveNodes():
                 AggregateCommand{factoryName(),
                                  commandName(),
                                  description()}
             { }
};
