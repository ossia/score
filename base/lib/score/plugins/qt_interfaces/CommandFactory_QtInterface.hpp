#pragma once
#include <score/command/Command.hpp>
#include <score/command/CommandGeneratorMap.hpp>
#include <score_lib_base_export.h>
#include <utility>

namespace score
{
class SCORE_LIB_BASE_EXPORT CommandFactory_QtInterface
{
public:
  virtual ~CommandFactory_QtInterface();

  virtual std::pair<const CommandGroupKey, CommandGeneratorMap> make_commands()
      = 0;
};
}
