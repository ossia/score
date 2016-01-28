#pragma once
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
class CommandTag{};
using CommandFactoryKey = StringKey<CommandTag>;
class CommandParentTag{};
using CommandParentFactoryKey = StringKey<CommandParentTag>;

template<typename T>
const CommandParentFactoryKey& CommandFactoryName();
