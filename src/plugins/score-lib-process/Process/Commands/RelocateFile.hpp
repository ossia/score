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

/** The single undoable step each file operation produces.
 *
 * One class per operation rather than one shared macro: they all bundle the
 * same kind of path rewrite, but the undo menu has to say which one happened.
 */
class SCORE_LIB_PROCESS_EXPORT ConsolidateProjectFiles final
    : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      Process::CommandFactoryName(), ConsolidateProjectFiles, "Consolidate project files")
};

class SCORE_LIB_PROCESS_EXPORT ReanchorProjectFiles final : public score::AggregateCommand
{
  // No hyphen in the description: score_generate_command_list_file scans for
  // these with a regex whose character class has no '-', so "Re-anchor" would
  // make the whole declaration invisible and the command unregistered.
  SCORE_COMMAND_DECL(
      Process::CommandFactoryName(), ReanchorProjectFiles, "Reanchor project files")
};

class SCORE_LIB_PROCESS_EXPORT RelinkProjectFiles final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      Process::CommandFactoryName(), RelinkProjectFiles, "Relink missing files")
};

class SCORE_LIB_PROCESS_EXPORT TrimProjectMedia final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(Process::CommandFactoryName(), TrimProjectMedia, "Trim media")
};
}
