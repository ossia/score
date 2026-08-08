#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <score_plugin_audio_export.h>

#include <functional>
#include <memory>

namespace ossia
{
class audio_engine;
}

namespace Audio
{
class AudioPreviewExecutor;
class SCORE_PLUGIN_AUDIO_EXPORT ApplicationPlugin final
    : public QObject
    , public score::GUIApplicationPlugin
{
public:
  ApplicationPlugin(const score::GUIApplicationContext& app);
  ~ApplicationPlugin() override;

  score::GUIElements makeGUIElements() override;

  std::shared_ptr<ossia::audio_engine> audio;

  /**
   * @brief Runs \p f with no audio engine holding the device, then brings the
   * engine back if one was running.
   *
   * Some backends need exclusive access to the hardware just to enumerate it --
   * ASIO in particular allows a single loaded driver per process, so probing the
   * installed drivers is impossible while one of them is streaming. Rescanning
   * from the settings therefore has to briefly take the engine down.
   *
   * Does not start an engine that was not running: a user who stopped audio
   * should not have it started behind their back.
   */
  void with_engine_stopped(std::function<void()> f);

private:
  void restart_engine();
  void stop_engine();
  void start_engine();

  QAction* m_audioEngineAct{};

  bool m_updating_audio = false;
  void initialize() override;

  void on_closeDocument(score::Document& old) override;

  void on_documentChanged(score::Document* olddoc, score::Document* newdoc) override;

  void timerEvent(QTimerEvent*) override;

  std::vector<std::shared_ptr<ossia::audio_engine>> previous_audio;
};

}
