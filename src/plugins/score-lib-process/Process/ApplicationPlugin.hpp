#pragma once
#include <Process/Preset.hpp>
#include <Process/Process.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>

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

  void addPreset(Process::Preset&& p);

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
};

}
