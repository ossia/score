#pragma once
#include <Library/LibraryInterface.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/tools/std/StringHash.hpp>

#include <QFileSystemModel>
#include <QMimeData>
#include <QSet>

#include <tsl/hopscotch_map.h>
#include <tsl/hopscotch_set.h>

namespace Library
{

class FileSystemModel : public QFileSystemModel
{
public:
  FileSystemModel(const score::GUIApplicationContext& ctx, QObject* parent)
      : QFileSystemModel{parent}
  {
    auto& lib_setup = ctx.interfaces<Library::LibraryInterfaceList>();
    // TODO refactor
    QSet<QString> types{// score-specific
                        "*.score",
                        "*.cue",
                        "*.device",
                        "*.oscquery",
                        "*.json"

                        // JS scripts
                        ,
                        "*.js",
                        "*.qml",
                        "*.script"

                        // Faust, other audio stuff
                        ,
                        "*.dsp",
                        "*.mid"};

    for (const LibraryInterface& lib : lib_setup)
    {
      auto lst = lib.acceptedFiles();
      for (const auto& ext : lib.acceptedFiles())
      {
        types.insert("*." + ext);
      }
    }

    setNameFilters(types.values());
    setNameFilterDisables(false);
    setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);

    for (auto& lib : lib_setup)
    {
      for (const auto& str : lib.acceptedMimeTypes())
      {
        m_mimeTypes.push_back(str);
        m_mimeActions[str] = &lib;
      }
    }
  }

  Qt::ItemFlags flags(const QModelIndex& index) const override
  {
    Qt::ItemFlags f
        = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;

    if (!isDir(index))
      f |= Qt::ItemIsEditable;

    return f;
  }

  QStringList mimeTypes() const override { return m_mimeTypes; }

  bool dropMimeData(
      const QMimeData* data,
      Qt::DropAction action,
      int row,
      int column,
      const QModelIndex& parent) override
  {
    LibraryInterface* handler{};
    for (const auto& fmt : data->formats())
    {
      auto it = m_mimeActions.find(fmt);
      if (it != m_mimeActions.end())
      {
        handler = it.value();
        break;
      }
    }

    if (!handler)
      return false;

    return handler->onDrop(*this, *data, row, column, parent);
  }

  QStringList m_mimeTypes;
  tsl::hopscotch_map<QString, LibraryInterface*> m_mimeActions;
};
}
