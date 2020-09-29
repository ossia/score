#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <score_plugin_audio_export.h>

#include <memory>

namespace ossia
{
class audio_engine;
}

namespace Audio
{
class AudioPreviewExecutor;
class SCORE_PLUGIN_AUDIO_EXPORT ApplicationPlugin final : public QObject,
                                                          public score::GUIApplicationPlugin
{
public:
  ApplicationPlugin(const score::GUIApplicationContext& app);
  ~ApplicationPlugin() override;

  score::GUIElements makeGUIElements() override;

  std::unique_ptr<ossia::audio_engine> audio;

private:
  void restart_engine();
  void stop_engine();
  void start_engine();

  QAction* m_audioEngineAct{};

  bool m_updating_audio = false;
  void initialize() override;

  void on_documentChanged(score::Document* olddoc, score::Document* newdoc) override;

  void timerEvent(QTimerEvent*) override;

};

}
