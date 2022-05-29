#pragma once
#include <Library/PresetListView.hpp>
#include <Library/ProcessTreeView.hpp>
#include <Library/LibraryInterface.hpp>

#include <score/tools/std/Optional.hpp>

#include <QSplitter>
#include <QTreeView>

#include <score_plugin_library_export.h>

#include <Process/Preset.hpp>
namespace score
{
struct GUIApplicationContext;
}
namespace Process
{
class ApplicationPlugin;
}
namespace Library
{
class ProcessesItemModel;
class PresetItemModel;

class PresetLibraryHandler final
    : public QObject
    , public Library::LibraryInterface
{
  SCORE_CONCRETE("7fc3a366-7792-489f-aca9-79d9f6d4415d")

public:
  QSet<QString> acceptedFiles() const noexcept override;
  QSet<QString> acceptedMimeTypes() const noexcept override;

  void setup(
      Library::ProcessesItemModel& model,
      const score::GUIApplicationContext& ctx) override;

  void addPath(std::string_view path) override;

  bool onDrop(
      const QMimeData& mime,
      int row,
      int column,
      const QDir& parent) override;

private:
  Process::ApplicationPlugin* presetLib{};
  const Process::ProcessFactoryList* processes{};
};

class SCORE_PLUGIN_LIBRARY_EXPORT ProcessWidget : public QWidget
{
public:
  ProcessWidget(const score::GUIApplicationContext& ctx, QWidget* parent);
  ~ProcessWidget();

  ProcessesItemModel& processModel() const noexcept { return *m_processModel; }
  PresetItemModel& presetModel() const noexcept { return *m_presetModel; }

  const ProcessTreeView& processView() const noexcept { return m_tv; }
  ProcessTreeView& processView() noexcept { return m_tv; }

  const PresetListView& presetView() const noexcept { return m_lv; }
  PresetListView& presetView() noexcept { return m_lv; }

private:
  ProcessesItemModel* m_processModel{};
  PresetItemModel* m_presetModel{};
  ProcessTreeView m_tv;
  PresetListView m_lv;

  QWidget m_preview;
  QWidget* m_previewChild{};
};
}
