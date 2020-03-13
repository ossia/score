#pragma once
#include <Library/LibraryInterface.hpp>

#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>

#include <Engine/ApplicationPlugin.hpp>

#include <Audio/AudioApplicationPlugin.hpp>

#include <ossia/audio/audio_protocol.hpp>

#include <Process/ExecutionContext.hpp>

#include <ossia/dataflow/nodes/sound_ref.hpp>

#include <Media/MediaFileHandle.hpp>

#include <Audio/Settings/Model.hpp>
#include <Audio/AudioPreviewExecutor.hpp>
#include <score/widgets/MarginLess.hpp>
namespace Media::Sound
{

class AudioPreviewWidget : public QWidget, public Nano::Observer
{
  //W_OBJECT(AudioPreviewWidget)
  AudioFile m_reader;
public:
    AudioPreviewWidget(const QString& path, QWidget* parent = nullptr)
      : QWidget{parent}
    {
      auto& audio = score::GUIAppContext().settings<Audio::Settings::Model>();
      int rate = audio.getRate();

      auto lay = new score::MarginLess<QHBoxLayout>{this};
      lay->addWidget(&m_playstop);
      lay->addWidget(&m_name);
      m_name.setText(QFileInfo{path}.fileName());
      m_reader.on_finishedDecoding.connect<&AudioPreviewWidget::on_finishedDecoding>(*this);
      m_reader.load(path, path, DecodingMethod::Libav);

      m_playstop.setText("⏵");
      m_playstop.setMinimumWidth(30);
      m_playstop.setMaximumWidth(30);

      connect(&m_playstop, &QPushButton::clicked,
              this, [&] {
        if(m_playstop.text() == "⏵")
        {
          setAutoPlay(true);
          startPlayback();
        }
        else
        {
          stopPlayback();
          setAutoPlay(false);
        }
      });
    }

    void on_finishedDecoding()
    {
      if(m_autoPlay)
      {
        m_playstop.setText("⏹");
        startPlayback();
      }
    }

    bool autoPlay() const noexcept { return m_autoPlay; }
    void setAutoPlay(bool b) { if(b != m_autoPlay) { m_autoPlay = b; } }

    void startPlayback()
    {
      auto& audio = score::GUIAppContext().settings<Audio::Settings::Model>();
      int rate = audio.getRate();

      auto& inst = Audio::AudioPreviewExecutor::instance();
      auto reader = m_reader.unsafe_handle().target<AudioFile::libav_ptr>();
      SCORE_ASSERT(reader);
      SCORE_ASSERT((*reader)->handle);
      inst.queue.enqueue({(*reader)->handle, (int)m_reader.channels(), (int)rate});

      m_playstop.setText("⏹");
    }

    void stopPlayback()
    {
      auto& inst = Audio::AudioPreviewExecutor::instance();
      inst.queue.enqueue({});
      m_playstop.setText("⏵");
    }

private:
  QPushButton m_playstop{this};
  QLabel m_name{this};
  static inline bool m_autoPlay{false};
};

class LibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("7d735f7f-d474-404d-8984-ba627e12f08c")

  QSet<QString> acceptedFiles() const noexcept override
  {
    return {
      "wav",
      "mp3",
      "mp4",
      "ogg",
      "flac",
      "aif",
      "aiff"
    };
  }

  QWidget* previewWidget(const QString& path, QWidget* parent) const noexcept override
  {
    return new AudioPreviewWidget{path, parent};
  }
};

}
