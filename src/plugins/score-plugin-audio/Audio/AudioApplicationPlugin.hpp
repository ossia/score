#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <memory>
#include <score_plugin_engine_export.h>

namespace ossia
{
class audio_engine;
}

namespace Audio
{
class SCORE_PLUGIN_ENGINE_EXPORT ApplicationPlugin final
    : public QObject,
      public score::GUIApplicationPlugin
{
public:
  ApplicationPlugin(const score::GUIApplicationContext& app);
  ~ApplicationPlugin() override;

  bool handleStartup() override;
  score::GUIElements makeGUIElements() override;

  std::unique_ptr<ossia::audio_engine> audio;

private:
  QAction* m_audioEngineAct{};

  bool m_updating_audio = false;
  void restart_engine();
  void setup_engine();
  void initialize();
  void on_stop();
};
}
