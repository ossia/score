#pragma once
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QVersionNumber>

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <vector>

// The parts of the JIT's compiler invocation that only depend on what is on
// disk and on what score was built with, so that they can be tested on a
// fake tree.
namespace Jit
{

//! The entry of <clangDir> (a lib/clang directory) that is the resource
//! directory of a clang <major>. LLVM 16 and later name it after the major
//! version only, older ones after the full version. Empty when none is of
//! that major: the headers and runtimes of another clang do not match the
//! one the JIT is built with.
inline QString selectClangResourceDir(
    const QString& clangDir, int major, const QString& fullVersion)
{
  const QStringList entries
      = QDir(clangDir).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  const QString majorName = QString::number(major);
  if(entries.contains(majorName))
    return majorName;
  if(!fullVersion.isEmpty() && entries.contains(fullVersion))
    return fullVersion;

  // e.g. 15.0.7 for a JIT built against 15.0.6
  QString best;
  QVersionNumber bestVersion;
  for(const QString& e : entries)
  {
    if(!e.startsWith(majorName + '.'))
      continue;
    const auto v = QVersionNumber::fromString(e);
    if(best.isEmpty() || v > bestVersion)
    {
      best = e;
      bestVersion = v;
    }
  }
  return best;
}

//! The newest resource directory of <clangDir>, for when none matches the
//! JIT's clang
inline QString newestClangResourceDir(const QString& clangDir)
{
  QString best;
  QVersionNumber bestVersion;
  for(const QString& e :
      QDir(clangDir).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
  {
    const auto v = QVersionNumber::fromString(e);
    if(v.isNull())
      continue;
    if(best.isEmpty() || v > bestVersion)
    {
      best = e;
      bestVersion = v;
    }
  }
  return best;
}

//! Looks for a compiler-rt archive matching <filter> in the resource
//! directory of clang <major> under each of <roots>/lib/clang, in order.
//! Within a root, the first archive whose path contains one of <preferred>
//! (in the order of <preferred>) wins over the others, which are taken in
//! name order.
inline QString findClangRuntimeArchive(
    const QStringList& roots, int major, const QString& fullVersion,
    const QString& filter, const QStringList& preferred = {})
{
  for(const QString& root : roots)
  {
    const QString clangDir = root + "/lib/clang";
    const QString version = selectClangResourceDir(clangDir, major, fullVersion);
    if(version.isEmpty())
      continue;

    // Debian's /usr/lib/clang/<v>/lib links to /usr/lib/llvm-<v>/lib/clang/<v>/lib
    QStringList found;
    QDirIterator it(
        clangDir + "/" + version, {filter}, QDir::Files,
        QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while(it.hasNext())
      found.push_back(it.next());
    if(found.empty())
      continue;
    found.sort();

    for(const QString& p : preferred)
      for(const QString& f : found)
        if(QString(f).replace('\\', '/').contains(p))
          return f;
    return found.front();
  }
  return {};
}

//! <triple> without its vendor, as Debian names its multiarch directories:
//! x86_64-pc-linux-gnu is x86_64-linux-gnu there
inline QString withoutVendor(const QString& triple)
{
  const QStringList parts = triple.split('-');
  if(parts.size() == 4)
    return parts[0] + '-' + parts[2] + '-' + parts[3];
  return triple;
}

//! The subdirectory of <includeRoot> that contains <marker> (e.g.
//! "c++/15/bits/c++config.h") for the first of <triples> that has one.
//! With multilib, /usr/include holds one such directory per architecture:
//! the one of another architecture must not be picked. When none matches a
//! triple, a single candidate is taken, several are left alone.
inline QString selectMultiarchDir(
    const QString& includeRoot, const QString& marker,
    const std::vector<QString>& triples)
{
  QStringList found;
  for(const QString& d :
      QDir(includeRoot).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
    if(QFileInfo::exists(includeRoot + "/" + d + "/" + marker))
      found.push_back(d);

  for(const QString& t : triples)
  {
    if(found.contains(t))
      return t;
    if(const QString v = withoutVendor(t); found.contains(v))
      return v;
  }
  if(found.size() == 1)
    return found.front();
  return {};
}

//! The include arguments for the include directories of score's build tree.
//! A directory of score's sources or build is searched as a user one, the
//! others are system ones. A macOS framework (e.g. Qt's QtCore.framework) is
//! not an include directory: <QtCore/qglobal.h> is looked up in its
//! Headers, which clang only does for the directory that contains it given
//! as a framework directory.
inline std::vector<std::string> buildTreeIncludeArgs(
    const std::vector<std::string_view>& dirs, std::string_view sourceTree,
    std::string_view binaryTree, const std::string& sdkInclude)
{
  std::vector<std::string> args;
  const QString cleanSdkInclude = QDir::cleanPath(QString::fromStdString(sdkInclude));
  auto add = [&](std::string arg) {
    if(std::find(args.begin(), args.end(), arg) == args.end())
      args.push_back(std::move(arg));
  };

  for(std::string_view dir : dirs)
  {
    if(dir.empty())
      continue;
    const QString clean
        = QDir::cleanPath(QString::fromUtf8(dir.data(), qsizetype(dir.size())));
    if(clean == cleanSdkInclude)
      continue;

    if(clean.endsWith(".framework"))
    {
      add("-iframework" + QFileInfo(clean).path().toStdString());
      continue;
    }

    std::string path(dir);
    if(path.starts_with(sourceTree) || path.starts_with(binaryTree))
      add("-I" + path);
    else
      add("-isystem" + path);
  }
  return args;
}

//! Removes the -D that repeat a definition already in effect, e.g. the
//! -D_GNU_SOURCE=1 every JIT compilation has, which score's build tree also
//! gives as -D_GNU_SOURCE. A definition with another value, or one after a
//! -U of the same macro, is kept: it is what the command line means.
inline void removeRepeatedDefinitions(std::vector<std::string>& args)
{
  // Macro name -> value of the definition in effect
  std::map<std::string, std::string, std::less<>> defined;
  std::vector<std::string> out;
  out.reserve(args.size());
  for(auto& arg : args)
  {
    if(arg.size() > 2 && arg.starts_with("-D"))
    {
      std::string_view def = std::string_view(arg).substr(2);
      const auto eq = def.find('=');
      std::string name(def.substr(0, eq));
      std::string value(eq == def.npos ? "1" : def.substr(eq + 1));
      if(auto it = defined.find(name); it != defined.end() && it->second == value)
        continue;
      defined[std::move(name)] = std::move(value);
    }
    else if(arg.size() > 2 && arg.starts_with("-U"))
    {
      defined.erase(arg.substr(2));
    }
    out.push_back(std::move(arg));
  }
  args = std::move(out);
}
}
