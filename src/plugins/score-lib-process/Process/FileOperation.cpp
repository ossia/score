#include <Process/Commands/RelocateFile.hpp>
#include <Process/FileOperation.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <QObject>

namespace Process
{

QString toString(FileAction a) noexcept
{
  switch(a)
  {
    case FileAction::Unchanged:
      return QObject::tr("Unchanged");
    case FileAction::Collect:
      return QObject::tr("Collect");
    case FileAction::AlreadyThere:
      return QObject::tr("Already in project");
    case FileAction::KeptInLibrary:
      return QObject::tr("Kept in library");
    case FileAction::Relinked:
      return QObject::tr("Relink");
    case FileAction::Trimmed:
      return QObject::tr("Trim");
    case FileAction::Unused:
      return QObject::tr("Unused");
    case FileAction::Removed:
      return QObject::tr("Removed");
    case FileAction::Missing:
      return QObject::tr("MISSING");
    case FileAction::Unsupported:
      return QObject::tr("External dependency");
    case FileAction::Skipped:
      return QObject::tr("Skipped");
    case FileAction::Failed:
      return QObject::tr("FAILED");
  }
  return {};
}

int FileReport::count(FileAction a) const noexcept
{
  int n = 0;
  for(const auto& e : entries)
    if(e.action == a)
      ++n;
  return n;
}

std::vector<const FileEntry*> FileReport::with(FileAction a) const noexcept
{
  std::vector<const FileEntry*> out;
  for(const auto& e : entries)
    if(e.action == a)
      out.push_back(&e);
  return out;
}

qint64 FileReport::bytesToCopy() const noexcept
{
  qint64 total = 0;
  for(const auto& e : entries)
    if(e.copyNeeded)
      total += e.size;
  return total;
}

qint64 FileReport::bytesSaved() const noexcept
{
  qint64 total = 0;
  for(const auto& e : entries)
    if(e.action == FileAction::Trimmed)
      total += e.size - e.newSize;
  return total;
}

FileReport runFileOperation(
    const score::DocumentContext& ctx, const FilePolicy& policy, bool dryRun)
{
  FileReport report;

  ExternalFileMap map{[&](const ExternalFileRef& ref) -> QString {
    FileEntry entry;
    entry.storedPath = ref.path;
    entry.kind = ref.kind;
    entry.usage = ref.usage;
    entry.owner = ref.owner;

    const QString next = policy(ref, entry);
    report.entries.push_back(std::move(entry));

    // A dry run reports everything and rewrites nothing: the processes are
    // never even asked to build their relocation commands.
    return dryRun ? QString{} : next;
  }};

  mapDocumentExternalFiles(ctx, map);

  if(!dryRun && map.hasCommands())
  {
    auto* macro = new Process::ConsolidateProjectFiles;
    for(auto* cmd : map.takeCommands())
      macro->addCommand(cmd);

    CommandDispatcher<>{ctx.commandStack}.submit(macro);
  }

  return report;
}
}
