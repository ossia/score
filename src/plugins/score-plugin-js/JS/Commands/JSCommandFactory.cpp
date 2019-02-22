// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "JSCommandFactory.hpp"

#include <score/command/Command.hpp>

const CommandGroupKey& JS::CommandFactoryName()
{
  static const CommandGroupKey key{"JS"};
  return key;
}
