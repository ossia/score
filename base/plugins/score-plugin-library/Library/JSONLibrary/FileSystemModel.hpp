#pragma once
#include <Library/LibraryInterface.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/tools/std/StringHash.hpp>
#include <hopscotch_set.h>
#include <hopscotch_map.h>
#include <QFileSystemModel>
#include <QMimeData>

namespace Library
{

class FileSystemModel
    : public QFileSystemModel
{
public:
  FileSystemModel(const score::GUIApplicationContext& ctx, QObject* parent)
    : QFileSystemModel{parent}
  {
    auto& lib_setup = ctx.interfaces<Library::LibraryInterfaceList>();
    // TODO refactor
    tsl::hopscotch_set<QString> types{
      // Media
       "*.wav"
     , "*.mp3"
     , "*.mp4"
     , "*.ogg"
     , "*.flac"
     , "*.aif"
     , "*.aiff"

      // score-specific
     , "*.score"
     , "*.cue"
     , "*.device"
     , "*.oscquery"
     , "*.json"

      // JS scripts
     , "*.js"
     , "*.qml"
     , "*.script"

      // Faust, other audio stuff
     , "*.dsp"
     , "*.mid"
    };

    for(auto& lib : lib_setup)
    {
      auto lst = lib.acceptedFiles();
      types.insert(lst.begin(), lst.end());
    }

    QStringList lst;
    for(const auto& s : types)
      lst.append(s);
    setNameFilters(lst);

    setNameFilterDisables(false);
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

  Qt::ItemFlags flags(const QModelIndex& index) const override
  {
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    return f;
  }

  QStringList mimeTypes() const override
  {
    return m_mimeTypes;
  }

  bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) override
  {
    LibraryInterface* handler{};
    for(const auto& fmt : data->formats())
    {
      auto it = m_mimeActions.find(fmt);
      if(it != m_mimeActions.end())
      {
        handler = it.value();
        break;
      }
    }

    if(!handler)
      return false;

    return handler->onDrop(*this, *data, row, column, parent);
  }

  QStringList m_mimeTypes;
  tsl::hopscotch_map<QString, LibraryInterface*> m_mimeActions;

};


}
