#pragma once
#include <Midi/Commands/MoveNotes.hpp>
#include <Midi/MidiProcess.hpp>
#include <Process/LayerPresenter.hpp>

#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>

#include <nano_observer.hpp>
class QMimeData;
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
      const Process::Context& ctx,
      QObject* parent);
  ~Presenter() override;

  void setWidth(qreal width, qreal defaultWidth) override;
  void setHeight(qreal height) override;

  void putToFront() override;
  void putBehind() override;

  void on_zoomRatioChanged(ZoomRatio) override;

  void parentGeometryChanged() override;

  const Midi::ProcessModel& model() const noexcept;

  void on_deselectOtherNotes();
  void on_noteChanged(NoteView& v);
  void on_noteChangeFinished(NoteView& v);
  void on_noteScaled(const Note& note, double newScale);
  void on_requestVelocityChange(const Note& note, double velocityDelta);
  void on_velocityChangeFinished();


private:
  void updateNote(NoteView&);
  void on_noteAdded(const Note&);
  void on_noteRemoving(const Note&);
  void on_drop(const QPointF& pos, const QMimeData&);

  std::vector<Id<Note>> selectedNotes() const;

  View* m_view{};
  std::vector<NoteView*> m_notes;

  SingleOngoingCommandDispatcher<MoveNotes> m_moveDispatcher;
  SingleOngoingCommandDispatcher<ChangeNotesVelocity> m_velocityDispatcher;
  ZoomRatio m_zr{};
  void fillContextMenu(
      QMenu& menu,
      QPoint pos,
      QPointF scenepos,
      const Process::LayerContextMenuManager& cm) override;
};
}
