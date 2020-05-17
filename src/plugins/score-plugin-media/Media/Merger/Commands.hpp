#pragma once
#include <Media/Commands/MediaCommandFactory.hpp>
#include <Media/Merger/Model.hpp>

#include <score/command/Command.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>

namespace Media
{
class SetMergeInCount final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(Media::CommandFactoryName(), SetMergeInCount, "Set in count")
public:
  SetMergeInCount(const Merger::Model& path, std::size_t newval)
      : score::PropertyCommand{std::move(path), "inCount", QVariant::fromValue(newval)}
  {
  }
};
}
