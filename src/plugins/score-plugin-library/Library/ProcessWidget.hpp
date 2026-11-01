#pragma once
#include <Process/Preset.hpp>

#include <Library/LibraryInterface.hpp>
#include <Library/PresetAudition.hpp>
#include <Library/PresetTreeView.hpp>
#include <Library/ProcessTreeView.hpp>

#include <score/tools/std/Optional.hpp>

#include <QLabel>
#include <QSplitter>
#include <QTreeView>

#include <functional>

#include <score_plugin_library_export.h>
namespace score
{
struct GUIApplicationContext;
}
namespace Process
{
class ApplicationPlugin;
class ProcessModel;
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

  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override;

  void addPath(std::string_view path) override;

  bool onDrop(const QMimeData& mime, int row, int column, const QDir& parent) override;

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

  const PresetTreeView& presetView() const noexcept { return m_presets; }
  PresetTreeView& presetView() noexcept { return m_presets; }

  //! Put a line of rich text above the process list, or take it away when the
  //! text is empty. Anything the user has to be told about the library itself
  //! belongs here rather than in a dialog: it is what they are looking at.
  //! A link in the text calls @p onActivated instead of opening a browser.
  void setNotice(const QString& html, std::function<void()> onActivated = {});

  //! The presets of this process, which load into it: what its preset button
  //! opens
  void showPresets(const Process::ProcessModel& proc);
  void clearTarget();

private:
  void raisePanel();

  ProcessesItemModel* m_processModel{};
  PresetItemModel* m_presetModel{};
  ProcessTreeView m_tv;
  PresetTreeView m_presets;
  QLabel m_notice;
  std::function<void()> m_noticeAction;
  QLineEdit* m_processFilter{};

  //! "Presets of <process>", above them while they load into it
  QWidget m_targetBar;
  QLabel m_targetLabel;
  QMetaObject::Connection m_targetGone;
  //! Presets loaded live into the target as the user moves through them
  PresetAudition m_audition;
  //! Set while the process tree is moved to the target's entry
  bool m_choosingTarget{};

  QWidget m_preview;
  QWidget* m_previewChild{};
  //! Rebuilds the preset preview for the row that is current, when one is.
  std::function<void()> m_refreshPresetPreview;
};
}
