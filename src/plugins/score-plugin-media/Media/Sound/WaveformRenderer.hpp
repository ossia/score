#pragma once
#include <Media/Sound/WaveformComputer.hpp>

#include <QObject>
#include <QRectF>
#include <QString>
#include <QVector>

#include <nano_observer.hpp>
#include <score_plugin_media_export.h>

#include <verdigris>

#include <memory>
#include <optional>

class QImage;
class QPainter;

namespace Media
{
class AudioFile;
}

namespace Media::Sound
{
/**
 * The waveform of a sound file, for a widget that draws it: the audio file
 * manager decodes the file, a WaveformComputer renders it into images, and
 * paint() draws them.
 *
 * Images are requested once the file is decoded, and again whenever the size,
 * pixel ratio or range change. They come back asynchronously: updated() tells
 * the owner to repaint.
 */
class SCORE_PLUGIN_MEDIA_EXPORT WaveformRenderer final
    : public QObject
    , public Nano::Observer
{
  W_OBJECT(WaveformRenderer)

public:
  //! \p threaded renders on a pool thread rather than on the GUI thread.
  explicit WaveformRenderer(QSizeF size, bool threaded = true, QObject* parent = nullptr);
  ~WaveformRenderer();

  //! \p path must be one the audio file manager can open: a resolved path, not
  //! a "<PROJECT>:" one. An empty path forgets the file.
  void setFile(const QString& path);
  const QString& path() const noexcept { return m_path; }
  const std::shared_ptr<AudioFile>& file() const noexcept { return m_file; }

  //! The part of the file that is shown, as fractions of its length.
  void setRange(double begin, double end);
  void setSize(QSizeF size, double devicePixelRatio);

  //! Whether there is a waveform to draw.
  bool hasWaveform() const noexcept { return !m_images.empty(); }

  //! Draws the waveform in \p rect, one band per channel. The rect's size and
  //! the pixel ratio become the ones the next images are rendered for.
  void paint(QPainter& painter, const QRectF& rect, double devicePixelRatio);

  void updated() E_SIGNAL(SCORE_PLUGIN_MEDIA_EXPORT, updated);

private:
  void on_finishedDecoding();
  void request();
  void clearImages();

  QString m_path;
  std::shared_ptr<AudioFile> m_file;
  WaveformComputer* m_computer{};

  QVector<QImage*> m_images;
  ComputedWaveform m_wf{};
  std::optional<WaveformRequest> m_lastRequest;

  QSizeF m_size{};
  double m_dpr{1.};
  double m_begin{0.};
  double m_end{1.};

  // Width of the whole file at the zoom of the last request, and that zoom.
  double m_layerWidth{};
  double m_zoom{};
};
}
