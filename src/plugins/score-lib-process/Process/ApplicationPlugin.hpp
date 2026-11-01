#pragma once
#include <Process/Preset.hpp>
#include <Process/Process.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <QPointer>

#include <Process/Layer/LayerContextMenu.hpp>

namespace Process
{

class SCORE_LIB_PROCESS_EXPORT ApplicationPlugin
    : public QObject
    , public score::ApplicationPlugin
{
  W_OBJECT(ApplicationPlugin)
public:
  explicit ApplicationPlugin(const score::ApplicationContext& ctx);
  ~ApplicationPlugin();

  void savePreset(const Process::ProcessModel* proc)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, savePreset, proc)
  //! The preset button of this process was clicked: the library shows its
  //! presets
  void showPresets(const Process::ProcessModel* proc)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, showPresets, proc)
  //! Another preset was loaded into this process (Process::loadPreset)
  void currentPresetChanged(const Process::ProcessModel* proc)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, currentPresetChanged, proc)

  //! The process the library's presets load into, if any: drawn as such
  const Process::ProcessModel* presetTarget() const noexcept { return m_presetTarget; }
  void setPresetTarget(const Process::ProcessModel* proc);
  void presetTargetChanged(const Process::ProcessModel* proc)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, presetTargetChanged, proc)

  //! Emitted around the sorted insertion done by addPreset, so item models
  //! over `presets` (Library::PresetItemModel) can emit exact insert ranges.
  void presetAboutToBeAdded(int index)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, presetAboutToBeAdded, index)
  void presetAdded(int index) E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, presetAdded, index)
  void presetAboutToBeRemoved(int index)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, presetAboutToBeRemoved, index)
  void presetRemoved(int index) E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, presetRemoved, index)
  //! Its name or file changed; it keeps its place, sorted by key only
  void presetChanged(int index) E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, presetChanged, index)
  void presetsAboutToBeCleared() E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, presetsAboutToBeCleared)
  void presetsCleared() E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, presetsCleared)

  void addPreset(Process::Preset&& p);
  void removePreset(int index);
  //! Before the library reads its folders again
  void clearPresets();

  Process::LayerContextMenuManager& layerContextMenuRegistrar()
  {
    return m_layerCtxMenuManager;
  }

  const Process::LayerContextMenuManager& layerContextMenuRegistrar() const
  {
    return m_layerCtxMenuManager;
  }

  std::vector<Process::Preset> presets;
  Process::LayerContextMenuManager m_layerCtxMenuManager;

private:
  QPointer<const Process::ProcessModel> m_presetTarget;
};

}
