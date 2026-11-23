#pragma once
#include <Recording/Commands/RecordingCommandFactory.hpp>

#include <score/command/AggregateCommand.hpp>

namespace Recording
{
//! Everything a recording session placed in the document: one undoable step.
class PlaceTakes final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(RecordingCommandFactoryName(), PlaceTakes, "Record takes")
};
}
