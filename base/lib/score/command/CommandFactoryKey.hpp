#pragma once
#include <score/plugins/StringFactoryKey.hpp>
class CommandTag
{
};
class CommandParentTag
{
};

using CommandKey = StringKey<CommandTag>;
using CommandGroupKey = StringKey<CommandParentTag>;

template <typename T>
const CommandGroupKey& CommandFactoryName();
