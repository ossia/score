#pragma once
class CommandTag
{
};
class CommandParentTag
{
};
template <typename Tag>
class StringKey;
using CommandKey = StringKey<CommandTag>;
using CommandGroupKey = StringKey<CommandParentTag>;

template <typename T>
const CommandGroupKey& CommandFactoryName();
