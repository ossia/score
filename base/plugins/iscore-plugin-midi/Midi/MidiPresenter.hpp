#pragma once
#include <Process/LayerPresenter.hpp>
#include <nano_observer.hpp>

namespace Midi
{
class NoteView;
class View;
class Note;
class Presenter final :
        public Process::LayerPresenter,
        public Nano::Observer
{
    public:
        explicit Presenter(
                const Layer& model,
                View* view,
                const Process::ProcessPresenterContext& ctx,
                QObject* parent);

        void setWidth(qreal width) override;
        void setHeight(qreal height) override;

        void putToFront() override;
        void putBehind() override;

        void on_zoomRatioChanged(ZoomRatio) override;

        void parentGeometryChanged() override;

        const Process::LayerModel& layerModel() const override;
        const Id<Process::ProcessModel>& modelId() const override;

    private:
        void setupNote(NoteView&);
        void on_noteAdded(const Note&);
        void on_noteRemoving(const Note&);
        void recompute();

        const Layer& m_layer;
        View* m_view{};
        std::vector<NoteView*> m_notes;
};
}
