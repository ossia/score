#pragma once
#include <Library/LibraryInterface.hpp>
#include <Media/MediaFileHandle.hpp>
#include <Process/ExecutionContext.hpp>

#include <score/widgets/MarginLess.hpp>
#include <score/widgets/Pixmap.hpp>

#include <ossia/audio/audio_protocol.hpp>
#include <ossia/dataflow/nodes/sound_ref.hpp>

#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

#include <Audio/AudioApplicationPlugin.hpp>
#include <Audio/AudioPreviewExecutor.hpp>
#include <Audio/Settings/Model.hpp>
#include <Engine/ApplicationPlugin.hpp>

namespace Media::Sound
{

class AudioPreviewWidget : public QWidget, public Nano::Observer
{
  // W_OBJECT(AudioPreviewWidget)
  AudioFile m_reader;

public:
  AudioPreviewWidget(const QString& path, QWidget* parent = nullptr) : QWidget{parent}
  {
    auto& audio = score::GUIAppContext().settings<Audio::Settings::Model>();

    auto lay = new score::MarginLess<QHBoxLayout>{this};
    lay->setSpacing(8);
    lay->addWidget(&m_playstop);
    lay->addWidget(&m_name);
    m_name.setText(QFileInfo{path}.fileName());
    m_reader.on_finishedDecoding.connect<&AudioPreviewWidget::on_finishedDecoding>(*this);
    m_reader.load(path, path, DecodingMethod::Libav);

    m_playPixmap = score::get_pixmap(":/icons/play_off.png");
    m_stopPixmap = score::get_pixmap(":/icons/stop_off.png");

    m_playstop.setIcon(m_playPixmap);

    m_playstop.setMinimumWidth(24);
    m_playstop.setMaximumWidth(24);

    connect(&m_playstop, &QPushButton::clicked, this, [&]() {
      if (!m_autoPlay)
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
    if (m_autoPlay)
    {
      m_playstop.setChecked(false);
      startPlayback();
    }
  }

  bool autoPlay() const noexcept { return m_autoPlay; }
  void setAutoPlay(bool b)
  {
    if (b != m_autoPlay)
    {
      m_autoPlay = b;
    }
  }

  void startPlayback()
  {
    auto& audio = score::GUIAppContext().settings<Audio::Settings::Model>();
    int rate = audio.getRate();

    auto& inst = Audio::AudioPreviewExecutor::instance();
    auto reader = m_reader.unsafe_handle().target<AudioFile::libav_ptr>();
    SCORE_ASSERT(reader);
    SCORE_ASSERT((*reader)->handle);
    inst.queue.enqueue({(*reader)->handle, (int)m_reader.channels(), (int)rate});

    m_playstop.setIcon(m_stopPixmap);
  }

  void stopPlayback()
  {
    auto& inst = Audio::AudioPreviewExecutor::instance();
    inst.queue.enqueue({});

    m_playstop.setIcon(m_playPixmap);
  }

private:
  QPushButton m_playstop{this};
  QLabel m_name{this};
  QPixmap m_playPixmap;
  QPixmap m_stopPixmap;

  static inline bool m_autoPlay{false};
};

class LibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("7d735f7f-d474-404d-8984-ba627e12f08c")

  QSet<QString> acceptedFiles() const noexcept override
  {
    return {"wav", "mp3", "m4a", "ogg", "flac", "aif", "aiff"};
  }

  QWidget* previewWidget(const QString& path, QWidget* parent) const noexcept override
  {
    return new AudioPreviewWidget{path, parent};
  }
};

}
