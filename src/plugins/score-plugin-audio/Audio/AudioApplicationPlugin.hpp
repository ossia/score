#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <score_plugin_audio_export.h>

#include <QPointer>

#include <memory>

namespace ossia
{
class audio_engine;
class audio_parameter;
}
namespace score
{
struct VolumeSlider;
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

private:
  void restart_engine();
  void stop_engine();
  void start_engine();
  void rebind_engine(score::Document& doc);
  //! The master volume of a document: the gain of its /out/main.
  ossia::audio_parameter* mainOutput(const score::DocumentContext* doc) const;
  void syncVolume();

  QAction* m_audioEngineAct{};
  QPointer<score::VolumeSlider> m_volume;
  QMetaObject::Connection m_volumeSync;
  bool m_volumeDragging{};
  double m_shownVolume{-1.};

  bool m_updating_audio = false;
  void initialize() override;

  void on_closeDocument(score::Document& old) override;

  void on_documentChanged(score::Document* olddoc, score::Document* newdoc) override;

  void timerEvent(QTimerEvent*) override;

  std::vector<std::shared_ptr<ossia::audio_engine>> previous_audio;
};

}
