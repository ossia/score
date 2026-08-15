#pragma once
#include <score/tools/ProjectFiles.hpp>

#include <QString>

#include <score_lib_process_export.h>

#include <functional>
#include <memory>
#include <vector>

namespace score
{
class Command;
struct DocumentContext;
}

namespace Process
{
class ControlInlet;
class ProcessModel;

//! Whether the process reads the file or writes to it.
enum class FileUsage
{
  Input, //!< Read by the process: must be collected next to the document.
  Output //!< Written by the process (recorders, encoders): only the path moves.
};

//! One external file referenced by a document.
struct ExternalFileRef
{
  //! Exactly as stored in the model: absolute, document-relative,
  //! "<PROJECT>:..." or "<LIBRARY>:...".
  QString path;
  score::FileKind kind{score::FileKind::Unknown};
  FileUsage usage{FileUsage::Input};
  //! The reference designates a folder, not a file.
  bool directory{};
  //! score knows how to rewrite this reference. False for things like plug-in
  //! binaries, which are reported so the user can install them by hand.
  bool rewritable{true};
  //! Human-readable name of the object holding the reference.
  QString owner;
};

//! Called for every reference found. Return the path it should be rewritten
//! to, or an empty string to leave it alone.
using ExternalFileMapper = std::function<QString(const ExternalFileRef&)>;

/**
 * @brief Collects — and in the same pass rewrites — the files a document
 *        references.
 *
 * Analysis and relocation go through one traversal on purpose: a separate
 * "list the files" and "rewrite the files" pass drift apart as processes are
 * added, and the user then gets a plan that does not match what happened.
 * Pass a mapper that always returns {} to only look.
 *
 * Processes report their references by calling the helpers below from
 * Process::ProcessModel::mapExternalFiles. The base implementation already
 * covers every file-valued control port (Process::FileChooserBase,
 * Process::FolderChooser and list-of-path controls), which is what all
 * avendish-based processes use, so only processes holding a path outside of a
 * port need to override it.
 */
class SCORE_LIB_PROCESS_EXPORT ExternalFileMap
{
public:
  explicit ExternalFileMap(ExternalFileMapper mapper) noexcept;
  ~ExternalFileMap();

  ExternalFileMap(const ExternalFileMap&) = delete;
  ExternalFileMap& operator=(const ExternalFileMap&) = delete;

  //! Report a reference and ask the mapper what to do with it.
  //! Returns the replacement path, or {} if it must stay as it is.
  QString map(ExternalFileRef ref);

  //! A path (or list of paths) held in a control port's value.
  void control(
      Process::ControlInlet& inlet, score::FileKind kind,
      FileUsage usage = FileUsage::Input);

  //! A folder held in a control port's value.
  void folder(Process::ControlInlet& inlet);

  //! A path held in a QString Q_PROPERTY of a model object.
  template <typename T>
  void property(
      T& obj, const char* property, score::FileKind kind,
      FileUsage usage = FileUsage::Input);

  //! A reference score cannot relocate; reported to the user only.
  void readOnly(const QString& path, score::FileKind kind);

  //! Queue a rewrite the caller built itself, for models whose path is only
  //! reachable through a dedicated command.
  void addCommand(score::Command* cmd);

  //! Name attributed to the references reported next. Set by the traversal.
  QString owner;

  //! Every reference seen, in traversal order.
  std::vector<ExternalFileRef> refs;

  //! Rewrites the mapper asked for. Ownership is transferred to the caller
  //! through takeCommands().
  std::vector<score::Command*> takeCommands() noexcept;

  bool hasCommands() const noexcept { return !m_commands.empty(); }

private:
  ExternalFileMapper m_mapper;
  std::vector<score::Command*> m_commands;
};

//! Walk every process of the document and let it report its files.
SCORE_LIB_PROCESS_EXPORT
void mapDocumentExternalFiles(const score::DocumentContext& ctx, ExternalFileMap& map);

//! Processes that accept either a path or the script itself store both in the
//! same string. True when this one is a path to a file that exists.
SCORE_LIB_PROCESS_EXPORT
bool looksLikeExistingFile(
    const QString& value, const score::DocumentContext& ctx) noexcept;
}

#include <Process/Commands/RelocateFile.hpp>

namespace Process
{
template <typename T>
void ExternalFileMap::property(
    T& obj, const char* property, score::FileKind kind, FileUsage usage)
{
  const QString cur = obj.property(property).toString().trimmed();
  if(cur.isEmpty())
    return;

  const QString next
      = map({.path = cur,
             .kind = kind == score::FileKind::Unknown ? score::guessFileKind(cur) : kind,
             .usage = usage,
             .directory = false,
             .rewritable = true,
             .owner = owner});
  if(next.isEmpty() || next == cur)
    return;

  addCommand(new Process::RelocateFilePath{obj, QString::fromUtf8(property), next});
}
}
