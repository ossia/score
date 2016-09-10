#pragma once
#include <Process/LayerPresenter.hpp>
#include <Midi/MidiLayer.hpp>
#include <nano_observer.hpp>
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <Midi/Commands/MoveNotes.hpp>
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
        void updateNote(NoteView&);
        void on_noteAdded(const Note&);
        void on_noteRemoving(const Note&);

        std::vector<Id<Note>> selectedNotes() const;


        const Layer& m_layer;
        View* m_view{};
        std::vector<NoteView*> m_notes;

        SingleOngoingCommandDispatcher<MoveNotes> m_ongoing;
};
}
