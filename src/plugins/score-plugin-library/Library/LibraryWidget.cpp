// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LibraryWidget.hpp"

#include <Library/FileSystemModel.hpp>
#include <Library/LibraryInterface.hpp>
#include <Library/RecursiveFilterProxy.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <QClipboard>
#include <QDesktopServices>
#include <QFileInfo>
#include <QMenu>
#include <QUrl>

namespace Library
{

std::vector<LibraryInterface*> libraryInterface(const QString& path)
{
  static auto matches = [] {
    ossia::hash_multimap<QString, LibraryInterface*> exp;
    const auto& libs = score::GUIAppContext().interfaces<LibraryInterfaceList>();
    for(auto& lib : libs)
    {
      for(const auto& ext : lib.acceptedFiles())
      {
        exp.insert({ext, &lib});
      }
    }
    return exp;
  }();

  std::vector<LibraryInterface*> libs;
  auto [begin, end] = matches.equal_range(QFileInfo(path).suffix().toLower());

  for(auto it = begin; it != end; ++it)
  {
    libs.push_back(it->second);
  }
  return libs;
}

void setupFilesystemContextMenu(
    QTreeView& m_tv, FileSystemModel& model, FileSystemRecursiveFilterProxy& proxy)
{
  m_tv.connect(&m_tv, &QTreeView::customContextMenuRequested, &m_tv, [&](QPoint pos) {
    auto idx = m_tv.indexAt(pos);
    if(!idx.isValid())
      return;
    auto source = proxy.mapToSource(idx);
    if(!source.isValid())
      return;
    QFileInfo fi{model.filePath(source)};

    auto folder_path = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();

    auto menu = new QMenu{&m_tv};
    auto file_expl = menu->addAction(QObject::tr("Open in file explorer"));
    m_tv.connect(file_expl, &QAction::triggered, &m_tv, [=] {
      QDesktopServices::openUrl(QUrl::fromLocalFile(folder_path));
    });

    if constexpr(FileSystemModel::supportsDisablingSorting())
    {
      auto sorting = new QAction(QObject::tr("Sort"));
      sorting->setCheckable(true);
      sorting->setChecked(model.isSorting());
      menu->addAction(sorting);
      m_tv.connect(sorting, &QAction::triggered, &m_tv, [&model](bool checked) {
        model.setSorting(checked);
      });
    }

    auto copy_path = menu->addAction(QObject::tr("Copy path"));
    m_tv.connect(copy_path, &QAction::triggered, &m_tv, [=] {
      QGuiApplication::clipboard()->setText(fi.absoluteFilePath());
    });

    menu->exec(m_tv.mapToGlobal(pos));
    menu->deleteLater();
  });
}
}
