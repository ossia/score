// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <score/command/Command.hpp>
#include <score/command/CommandData.hpp>

score::CommandData::CommandData(const score::Command& cmd)
    : parentKey{cmd.parentKey()}, commandKey{cmd.key()}, data{cmd.serialize()}
{
}
