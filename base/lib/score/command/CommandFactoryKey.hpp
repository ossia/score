#pragma once
#include <score/plugins/customfactory/StringFactoryKey.hpp>
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
