#pragma once
#include <Process/Commands/ProcessCommandFactory.hpp>

#include <score/command/AggregateCommand.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/model/path/PathSerialization.hpp>

namespace Process
{
/**
 * @brief Points a QString path property at another file.
 *
 * Generic on purpose: project consolidation has to rewrite paths held by
 * processes it knows nothing about, and every one of them exposes its path as
 * a Q_PROPERTY.
 */
class SCORE_LIB_PROCESS_EXPORT RelocateFilePath final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(Process::CommandFactoryName(), RelocateFilePath, "Relocate a file")
public:
  using score::PropertyCommand::PropertyCommand;
};

//! The single undoable step a project consolidation produces.
class SCORE_LIB_PROCESS_EXPORT ConsolidateProjectFiles final
    : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      Process::CommandFactoryName(), ConsolidateProjectFiles, "Consolidate project files")
};
}
