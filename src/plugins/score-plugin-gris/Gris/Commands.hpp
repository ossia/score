#pragma once

#include <Gris/Model.hpp>

#include <score/command/Command.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>

namespace Gris
{
const CommandGroupKey& CommandFactoryName();

/** Changing the source count adds or removes ports, so it belongs on the undo
 *  stack rather than happening as a side effect of a control value. */
class SetSourceCount final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(Gris::CommandFactoryName(), SetSourceCount, "Set source count")
public:
  SetSourceCount(const SpatModel& model, int newCount)
      : score::PropertyCommand{model, "sourceCount", QVariant::fromValue(newCount)}
  {
  }
};
} // namespace Gris
