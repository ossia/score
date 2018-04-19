// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "RecordingCommandFactory.hpp"

#include <score/command/Command.hpp>

const CommandGroupKey& RecordingCommandFactoryName()
{
  static const CommandGroupKey key{"Cohesion"};
  return key;
}
