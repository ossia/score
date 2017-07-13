// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <iscore/command/CommandData.hpp>
#include <iscore/command/Command.hpp>

iscore::CommandData::CommandData(const iscore::Command& cmd)
    : parentKey{cmd.parentKey()}, commandKey{cmd.key()}, data{cmd.serialize()}
{
}
