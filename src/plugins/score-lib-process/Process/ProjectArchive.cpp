#include <Process/ProjectArchive.hpp>
#include <Process/ProjectConsolidation.hpp>

#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>

#include <QFileInfo>
#include <QSet>

namespace Process
{

std::vector<score::ZipEntry>
projectArchiveContents(const score::DocumentContext& ctx, const FileReport& report)
{
  const QString documentFile = ctx.document.metadata().fileName();
  const QFileInfo docInfo{documentFile};
  const QString folder
      = report.projectFolder.isEmpty() ? docInfo.absolutePath() : report.projectFolder;
  if(folder.isEmpty())
    return {};

  const QString root = docInfo.completeBaseName().isEmpty()
                           ? QStringLiteral("project")
                           : docInfo.completeBaseName();

  std::vector<score::ZipEntry> out;
  QSet<QString> seen;

  const auto add = [&](const QString& absolute) {
    if(absolute.isEmpty() || !QFileInfo{absolute}.isFile())
      return;
    if(!score::isUnderFolder(absolute, folder))
      return;

    QString relative = absolute.mid(folder.size());
    while(relative.startsWith('/'))
      relative.remove(0, 1);
    if(relative.isEmpty())
      return;

    const QString name = root + '/' + relative;
    if(seen.contains(name))
      return;
    seen.insert(name);
    out.push_back({absolute, name});
  };

  add(docInfo.absoluteFilePath());

  for(const auto& e : report.entries)
  {
    // Whatever the reference ends up pointing at: a file just collected, or
    // one that was already in the project and only had its path rewritten.
    add(e.destinationPath.isEmpty() ? e.sourcePath : e.destinationPath);
  }

  return out;
}

qint64 archiveContentsSize(const std::vector<score::ZipEntry>& entries) noexcept
{
  qint64 total = 0;
  for(const auto& e : entries)
    total += QFileInfo{e.sourceFile}.size();
  return total;
}
}
