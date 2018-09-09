#pragma once
#include <QFileSystemModel>
#include <QMimeData>

#include <score/application/GUIApplicationContext.hpp>
namespace Library
{

class FileSystemModel
    : public QFileSystemModel
{
public:
  FileSystemModel(const score::GUIApplicationContext& ctx, QObject* parent)
    : QFileSystemModel{parent}
  {
    setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    setNameFilters({
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
                   });
    setNameFilterDisables(false);
  }

  Qt::ItemFlags flags(const QModelIndex& index) const override
  {
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
    return f;
  }
};

}
