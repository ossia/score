#pragma once
#include <score/command/CommandGeneratorMap.hpp>
#include <score_lib_base_export.h>
#include <utility>

#include <score/command/Command.hpp>

namespace score
{
class SCORE_LIB_BASE_EXPORT CommandFactory_QtInterface
{
public:
  virtual ~CommandFactory_QtInterface();

  virtual std::pair<const CommandGroupKey, CommandGeneratorMap>
  make_commands() = 0;
};
}

#define CommandFactory_QtInterface_iid \
  "org.ossia.score.plugins.CommandFactory_QtInterface"

Q_DECLARE_INTERFACE(
    score::CommandFactory_QtInterface, CommandFactory_QtInterface_iid)
