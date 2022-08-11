#pragma once
#include <Library/LibraryInterface.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/tools/std/StringHash.hpp>
#include <score/widgets/IconProvider.hpp>

#include <QFileIconProvider>
#include <QFileSystemModel>
#include <QGuiApplication>
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
    setIconProvider(&score::IconProvider::instance());

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

    for(const LibraryInterface& lib : lib_setup)
    {
      auto lst = lib.acceptedFiles();
      for(const auto& ext : lib.acceptedFiles())
      {
        types.insert("*." + ext);
      }
    }

    // QFileSystemModel is case-sensitive, so we at least handle the ALL-CAPS EXTENSIONS
    // **floppy drive noises buzzing in the background**
    {
      auto copy = types;
      for(auto t : copy)
      {
        types.insert(t.toUpper());
      }
    }

    setNameFilters(types.values());
    setResolveSymlinks(true);
    setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);

    for(auto& lib : lib_setup)
    {
      for(const auto& str : lib.acceptedMimeTypes())
      {
        m_mimeTypes.push_back(str);
        m_mimeActions[str] = &lib;
      }
    }
  }

  static consteval bool supportsDisablingSorting() noexcept
  {
    return []<typename T>(T * fsm) constexpr
    {
      return requires
      {
        fsm->setOption(T::DontSort);
      };
    }
    ((QFileSystemModel*)nullptr);
  }

  void setSorting(bool b) noexcept
  {
    [b]<typename T>(T& self) {
      if constexpr(T::supportsDisablingSorting())
      {
        static_assert(T::supportsDisablingSorting());
        self.setOption(T::DontSort, b);
      }
        }(*this);
  }

  bool isSorting() noexcept
  {
    return []<typename T>(T& fsm) {
      if constexpr(supportsDisablingSorting())
      {
        static_assert(requires { fsm.setOption(T::DontSort); });
        return !fsm.testOption(T::DontSort);
      }
      else
      {
        return true;
      }
    }(*this);
  }

  Qt::ItemFlags flags(const QModelIndex& index) const override
  {
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled
                      | Qt::ItemIsDropEnabled;

    if(!isDir(index))
      f |= Qt::ItemIsEditable;

    return f;
  }

  QStringList mimeTypes() const override { return m_mimeTypes; }

  bool dropMimeData(
      const QMimeData* data, Qt::DropAction action, int row, int column,
      const QModelIndex& parent) override
  {
    const QFileInfo f = fileInfo(parent);
    const QDir d = f.isDir() ? QDir(f.canonicalFilePath()) : f.absoluteDir();
    for(const auto& fmt : data->formats())
    {
      auto it = m_mimeActions.find(fmt);
      if(it != m_mimeActions.end())
      {
        if(it.value()->onDrop(*data, row, column, d))
          return true;
      }
    }

    return false;
  }

  QVariant data(const QModelIndex& index, int role) const override
  {
    if(role == Qt::DecorationRole)
    {
      if(isDir(index))
        return score::IconProvider::folderIcon();
      else
        return {};
    }
    return QFileSystemModel::data(index, role);
  }

  QStringList m_mimeTypes;
  tsl::hopscotch_map<QString, LibraryInterface*> m_mimeActions;
};
}
