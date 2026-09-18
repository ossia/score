#pragma once
#include <optional>
#include <Process/LayerView.hpp>
#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>
#include <score_plugin_media_export.h>

#include <Media/AudioArray.hpp>
#include <Media/MediaFileHandle.hpp>
#include <Media/Sound/WaveformComputer.hpp>

#include <score/graphics/GraphicsItem.hpp>

#include <ossia/detail/pod_vector.hpp>

#include <verdigris>
namespace Media
{
namespace Sound
{
class ProcessModel;
class SCORE_PLUGIN_MEDIA_EXPORT LayerView final
    : public Process::LayerView
    , public Nano::Observer
{
public:
  explicit LayerView(const ProcessModel& model, QGraphicsItem* parent);
  ~LayerView();

  void setData(const std::shared_ptr<AudioFile>& data);

  //! The span of the layer the current image covers, in item coordinates.
  //! Empty when there is no image yet.
  QRectF renderedSpan() const noexcept;
  void setFrontColors(bool);
  void setTempoRatio(double);
  void recompute(ZoomRatio ratio);
  void recompute() const;

  void on_finishedDecoding();

private:
  void paint_impl(QPainter*) const override;
  void mousePressEvent(QGraphicsSceneMouseEvent*) override;
  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  void heightChanged(qreal) override;
  void widthChanged(qreal) override;

  void scrollValueChanged(int);

  void on_newData();

  std::shared_ptr<AudioFile> m_data;
  int m_numChan{};
  int m_sampleRate{};

  ZoomRatio m_zoom{};
  double m_tempoRatio{1.};

  QVector<QImage*> m_images;
  WaveformComputer* m_cpt{};

  ComputedWaveform m_wf{};
  const ProcessModel& m_model;

  void requestIfUncovered(double coveredX0, double coveredXf) const;

  bool m_frontColors{true};
  mutable bool m_recomputed{false};

  //! What was last sent to the computer, to avoid asking twice for the same
  //! image: once the whole layer is rendered, scrolling does not change the
  //! request at all.
  mutable std::optional<WaveformRequest> m_lastRequest;
};
}
}
