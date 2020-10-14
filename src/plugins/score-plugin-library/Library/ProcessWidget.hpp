#pragma once
#include <Library/PresetListView.hpp>
#include <Library/ProcessTreeView.hpp>

#include <score/tools/std/Optional.hpp>

#include <QSplitter>
#include <QTreeView>

#include <score_plugin_library_export.h>
namespace score
{
struct GUIApplicationContext;
}
namespace Library
{
class ProcessesItemModel;
class PresetItemModel;

class SCORE_PLUGIN_LIBRARY_EXPORT ProcessWidget : public QWidget
{
public:
  ProcessWidget(const score::GUIApplicationContext& ctx, QWidget* parent);
  ~ProcessWidget();

  ProcessesItemModel& processModel() const noexcept
  { return *m_processModel; }
  const ProcessTreeView& processView() const noexcept
  { return m_tv; }
  ProcessTreeView& processView() noexcept
  { return m_tv; }

private:
  ProcessesItemModel* m_processModel{};
  PresetItemModel* m_presetModel{};
  ProcessTreeView m_tv;
  PresetListView m_lv;
};
}
