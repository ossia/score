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
class GUIApplicationPlugin;
class AudioPreviewExecutor;
class SCORE_PLUGIN_AUDIO_EXPORT ApplicationPlugin final
    : public QObject
    , public score::ApplicationPlugin
{
  friend class GUIApplicationPlugin;

public:
  ApplicationPlugin(const score::ApplicationContext& app);
  ~ApplicationPlugin() override;

  std::shared_ptr<ossia::audio_engine> audio;

private:
  void restart_engine();
  void stop_engine();
  void start_engine();

  bool m_updating_audio = false;
  void initialize() override;

  void on_documentChanged(score::Document* olddoc, score::Document* newdoc) override;

  void timerEvent(QTimerEvent*) override;

  QAction* m_audioEngineAct{};
  std::vector<std::shared_ptr<ossia::audio_engine>> previous_audio;
};

class SCORE_PLUGIN_AUDIO_EXPORT GUIApplicationPlugin final
    : public QObject
    , public score::GUIApplicationPlugin
{
public:
  GUIApplicationPlugin(const score::GUIApplicationContext& app);
  ~GUIApplicationPlugin() override;

private:
  ApplicationPlugin& m_appPlug;

  void initialize() override;

  score::GUIElements makeGUIElements() override;
};
}
