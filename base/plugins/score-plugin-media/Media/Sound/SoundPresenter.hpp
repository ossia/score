#pragma once
#include <Process/LayerPresenter.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Media/Sound/SoundModel.hpp>
#include <Media/Sound/SoundLayer.hpp>
class QMimeData;
namespace Media
{
namespace Sound
{
class LayerView;

class LayerPresenter final :
        public Process::LayerPresenter
{
    public:
        using model_type = const Media::Sound::ProcessModel;
        explicit LayerPresenter(
                const ProcessModel& model,
                LayerView* view,
                const Process::ProcessPresenterContext& ctx,
                QObject* parent);

        void setWidth(qreal width) override;
        void setHeight(qreal height) override;

        void putToFront() override;
        void putBehind() override;

        void on_zoomRatioChanged(ZoomRatio) override;

        void parentGeometryChanged() override;

        const ProcessModel& model() const override;
        const Id<Process::ProcessModel>& modelId() const override;

    private:
        void onDrop(const QPointF& p, const QMimeData& mime);
        const ProcessModel& m_layer;
        LayerView* m_view{};
        ZoomRatio m_ratio{1};
};
}
}
