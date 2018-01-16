#pragma once
#include <Midi/Commands/MoveNotes.hpp>
#include <Midi/MidiProcess.hpp>
#include <Process/LayerPresenter.hpp>
#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <nano_observer.hpp>
namespace Midi
{
class NoteView;
class View;
class Note;
class Presenter final : public Process::LayerPresenter, public Nano::Observer
{
public:
  explicit Presenter(
      const Midi::ProcessModel& model,
      View* view,
      const Process::ProcessPresenterContext& ctx,
      QObject* parent);

  void setWidth(qreal width) override;
  void setHeight(qreal height) override;

  void putToFront() override;
  void putBehind() override;

  void on_zoomRatioChanged(ZoomRatio) override;

  void parentGeometryChanged() override;

  const Midi::ProcessModel& model() const override;
  const Id<Process::ProcessModel>& modelId() const override;

private:
  void setupNote(NoteView&);
  void updateNote(NoteView&);
  void on_noteAdded(const Note&);
  void on_noteRemoving(const Note&);

  std::vector<Id<Note>> selectedNotes() const;

  const Midi::ProcessModel& m_layer;
  View* m_view{};
  std::vector<NoteView*> m_notes;

  SingleOngoingCommandDispatcher<MoveNotes> m_ongoing;
  ZoomRatio m_zr{};
};
}
