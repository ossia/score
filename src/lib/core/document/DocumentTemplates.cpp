// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DocumentTemplates.hpp"

#include <score/tools/Zip.hpp>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

#include <algorithm>

namespace score
{
static const QStringList score_file_filters{"*.score", "*.scorejson", "*.zip"};
static const QString first_run_basename{"first-run"};

QString libraryRootPath()
{
  // FIXME we can't access Library::Settings::Model from the base library
  QSettings set;
  return set.value("Library/RootPath").toString();
}

namespace
{
struct SearchDir
{
  QString path;
  QString source;
};

std::vector<SearchDir> searchDirs(const QString& subfolder, bool bundled)
{
  std::vector<SearchDir> dirs;
  if(bundled)
    dirs.push_back({":/" + subfolder.toLower(), "bundled"});

  const auto root = libraryRootPath();
  if(root.isEmpty() || !QDir{root}.exists())
    return dirs;

  dirs.push_back({root + "/" + subfolder, "library"});

  QDir packages{root + "/packages"};
  for(const auto& pkg :
      packages.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable, QDir::Name))
  {
    dirs.push_back({packages.filePath(pkg) + "/" + subfolder, pkg});
  }
  return dirs;
}

void collect(const SearchDir& dir, bool recursive, std::vector<DocumentTemplate>& out)
{
  const QDir root{dir.path};
  if(!root.exists())
    return;

  // Hidden folders (the .git of a cloned package...) are not traversed
  QDirIterator it{
      dir.path, score_file_filters, QDir::Files | QDir::Readable,
      recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags};
  while(it.hasNext())
  {
    const QFileInfo fi{it.next()};
    if(fi.completeBaseName().compare(first_run_basename, Qt::CaseInsensitive) == 0)
      continue;

    // A zip is a project archive (score + media) only if it holds a score;
    // this reads the zip's table of contents, not its content.
    if(fi.suffix().compare("zip", Qt::CaseInsensitive) == 0
       && !summarizeZipArchive(fi.absoluteFilePath()))
      continue;

    QString category = root.relativeFilePath(fi.absolutePath());
    if(category == ".")
      category.clear();
    category.replace('/', " / ");
    out.push_back(
        {fi.completeBaseName(), fi.absoluteFilePath(), dir.source, std::move(category)});
  }
}

//! A package whose whole content is examples, e.g. a clone of ossia/score-examples.
bool isExamplesPackage(const QString& pkgPath)
{
  if(QFileInfo{pkgPath}.fileName().compare("examples", Qt::CaseInsensitive) == 0)
    return true;

  QFile f{pkgPath + "/package.json"};
  if(!f.open(QIODevice::ReadOnly))
    return false;
  const auto obj = QJsonDocument::fromJson(f.readAll()).object();
  return obj["kind"].toString().compare("examples", Qt::CaseInsensitive) == 0
         || obj["raw_name"].toString().compare("examples", Qt::CaseInsensitive) == 0;
}

//! A .score next to a .zip of the same name is the archive's content without
//! its media: only the archive is worth listing.
void dropScoresShadowedByArchives(std::vector<DocumentTemplate>& v)
{
  std::vector<QString> archives;
  for(const auto& t : v)
    if(t.path.endsWith(".zip", Qt::CaseInsensitive))
      archives.push_back(t.path.chopped(4));

  if(archives.empty())
    return;

  std::erase_if(v, [&](const DocumentTemplate& t) {
    if(t.path.endsWith(".zip", Qt::CaseInsensitive))
      return false;
    const QString base
        = QFileInfo{t.path}.path() + "/" + QFileInfo{t.path}.completeBaseName();
    return std::find(archives.begin(), archives.end(), base) != archives.end();
  });
}

void sortByCategoryAndName(std::vector<DocumentTemplate>& v)
{
  dropScoresShadowedByArchives(v);
  std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) {
    if(int c = a.category.compare(b.category, Qt::CaseInsensitive); c != 0)
      return c < 0;
    return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
  });
}
}

QString defaultDocumentTemplate()
{
  const auto root = libraryRootPath();
  if(root.isEmpty())
    return {};
  const QString path = root + "/default.score";
  return QFile::exists(path) ? path : QString{};
}

QString firstRunDocumentTemplate()
{
  for(const auto& dir : searchDirs("Templates", true))
  {
    for(const auto& ext : {".score", ".scorejson"})
    {
      const QString path = dir.path + "/" + first_run_basename + ext;
      if(QFile::exists(path))
        return path;
    }
  }
  return {};
}

std::vector<DocumentTemplate> availableDocumentTemplates()
{
  std::vector<DocumentTemplate> res;
  for(const auto& dir : searchDirs("Templates", true))
    collect(dir, false, res);
  sortByCategoryAndName(res);
  return res;
}

std::vector<DocumentTemplate> availableExampleDocuments()
{
  std::vector<DocumentTemplate> res;
  for(const auto& dir : searchDirs("Examples", false))
  {
    // dir.path is <pkg>/Examples: a package that is entirely made of examples
    // is scanned from its root instead
    const QString pkg = QFileInfo{dir.path}.path();
    if(dir.source != "library" && isExamplesPackage(pkg))
      collect({pkg, dir.source}, true, res);
    else
      collect(dir, true, res);
  }
  sortByCategoryAndName(res);
  return res;
}
}
