#pragma once
#include <Process/Preset.hpp>

#include <QListView>

#include <score_plugin_library_export.h>

#include <verdigris>

namespace Library
{
class SCORE_PLUGIN_LIBRARY_EXPORT PresetListView : public QListView
{
  W_OBJECT(PresetListView)
public:
  using QListView::QListView;
  void mouseDoubleClickEvent(QMouseEvent* event);

  void selected(std::optional<Process::Preset> p)
      E_SIGNAL(SCORE_PLUGIN_LIBRARY_EXPORT, selected, p)
  void doubleClicked(Process::Preset p)
      E_SIGNAL(SCORE_PLUGIN_LIBRARY_EXPORT, doubleClicked, p)
};

}
