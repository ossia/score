#pragma once
#include <score/tools/NaturalSort.hpp>

#include <QDir>
#include <QDirIterator>
#include <QStringList>

namespace score
{
//! The readable files of \p folder matching \p nameFilters ("*.wav"; all
//! files when empty), as paths relative to it, in natural order. What indexes
//! a folder's files and what shows them use this, so that the same number is
//! the same file in both.
inline QStringList
folderFiles(const QString& folder, const QStringList& nameFilters, bool recursive = false)
{
  QStringList files;
  if(folder.isEmpty())
    return files;
  const QDir root{folder};
  QDirIterator it{
      folder, nameFilters, QDir::Files | QDir::Readable,
      recursive ? QDirIterator::Subdirectories | QDirIterator::FollowSymlinks
                : QDirIterator::NoIteratorFlags};
  while(it.hasNext())
    files.push_back(root.relativeFilePath(it.next()));
  sortNaturally(files);
  return files;
}
}
