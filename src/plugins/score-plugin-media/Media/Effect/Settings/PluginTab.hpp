#pragma once

// Reusable UI for the per-format plug-in settings tabs (VST / VST3 / CLAP /
// LV2): an editable search-path list, a Rescan button and the two
// working / faulty plug-in tables. Each format supplies its data source and
// actions through PluginTabSpec and gets an identical tab in the "Effects"
// settings page.

#include <score_plugin_media_export.h>

#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

class QWidget;
class QObject;

namespace Media::Settings
{
struct PluginTabRow
{
  QString name;
  QString path;
  bool valid{};
};

struct PluginTabSpec
{
  //! Label of the paths editor, e.g. "VST3 paths"
  QString pathsLabel;

  //! Current search paths from the settings model
  std::function<QStringList()> getPaths;
  //! Commit an edited path list (through the settings command dispatcher so
  //! the dialog's Cancel button reverts it)
  std::function<void(QStringList)> commitPaths;
  //! Called with (context, callback); must invoke callback(paths) whenever
  //! the settings model's path list changes
  std::function<void(QObject*, std::function<void(QStringList)>)> onPathsChanged;

  //! Current plug-in list of the format's application plug-in
  std::function<std::vector<PluginTabRow>()> rows;
  //! Called with (context, callback); must invoke callback() whenever the
  //! plug-in list changes
  std::function<void(QObject*, std::function<void()>)> onPluginsChanged;

  //! Forget everything and scan the current paths again
  std::function<void()> rescan;
};

SCORE_PLUGIN_MEDIA_EXPORT
QWidget* makePluginSettingsWidget(PluginTabSpec spec);
}
