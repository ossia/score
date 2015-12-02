#include <iscore/command/CommandData.hpp>
#include <iscore/command/SerializableCommand.hpp>

iscore::CommandData::CommandData(const iscore::SerializableCommand& cmd):
    parentKey{cmd.parentKey()},
    commandKey{cmd.key()},
    data{cmd.serialize()}
{

}
