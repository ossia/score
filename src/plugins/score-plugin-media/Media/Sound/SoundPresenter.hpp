#pragma once
#include <Media/Sound/SoundModel.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
class QMimeData;
namespace Media
{
namespace Sound
{
class LayerView;

class LayerPresenter final : public Process::LayerPresenter
{
public:
  using model_type = const Media::Sound::ProcessModel;
  explicit LayerPresenter(
      const ProcessModel& model,
      LayerView* view,
      const Process::Context& ctx,
      QObject* parent);

  void setWidth(qreal width, qreal defaultWidth) override;
  void setHeight(qreal height) override;

  void putToFront() override;
  void putBehind() override;

  void on_zoomRatioChanged(ZoomRatio) override;

  void parentGeometryChanged() override;

private:
  void updateTempo();
  void onDrop(const QPointF& p, const QMimeData& mime);
  LayerView* m_view{};
  ZoomRatio m_ratio{1};
};
}
}
