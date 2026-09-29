// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/Layer/LayerContextMenu.hpp>

#include <Midi/Commands/AddNote.hpp>
#include <Midi/Commands/MoveNotes.hpp>
#include <Midi/Commands/RemoveNotes.hpp>
#include <Midi/Commands/ScaleNotes.hpp>
#include <Midi/MidiDrop.hpp>
#include <Midi/MidiNoteView.hpp>
#include <Midi/MidiPresenter.hpp>
#include <Midi/MidiProcess.hpp>
#include <Midi/MidiView.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/tools/Bind.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/math.hpp>

#include <boost/range/adaptor/transformed.hpp>

#include <QAction>
#include <QApplication>
#include <QGraphicsView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Midi::Presenter)

namespace Midi
{
Presenter::Presenter(
    const Midi::ProcessModel& layer, View* view, const Process::Context& ctx,
    QObject* parent)
    : LayerPresenter{layer, view, ctx, parent}
    , m_view{view}
    , m_moveDispatcher{ctx.commandStack}
    , m_velocityDispatcher{ctx.commandStack}
    , m_zr{1.}
{
  putToFront();

  auto& model = layer;

  con(
      model, &ProcessModel::durationChanged, this,
      [&] {
    for(auto [_, note] : m_notes)
      updateNote(*note);
      },
      Qt::QueuedConnection);
  con(model, &ProcessModel::notesNeedUpdate, this, [&] {
    for(auto [_, note] : m_notes)
      updateNote(*note);
  });

  con(model, &ProcessModel::notesChanged, this, [&] {
    for(auto [_, note] : m_notes)
    {
      delete note;
    }

    m_notes.clear();
    m_selectedNotes.clear();

    for(auto& note : model.notes)
    {
      on_noteAdded(note);
    }
  });

  con(model, &ProcessModel::rangeChanged, this, [this](int min, int max) {
    m_view->setRange(min, max);
    for(auto [_, note] : m_notes)
      updateNote(*note);
  });
  m_view->setRange(model.range().first, model.range().second);
  model.notes.added.connect<&Presenter::on_noteAdded>(this);
  model.notes.removing.connect<&Presenter::on_noteRemoving>(this);
  model.notes.replaced.connect<&Presenter::on_notesReplaced>(this);

  connect(m_view, &View::doubleClicked, this, [&](QPointF pos) {
    CommandDispatcher<>{context().context.commandStack}.submit(
        new AddNote{layer, m_view->noteAtPos(pos)});
  });

  connect(m_view, &View::pressed, this, [&] {
    for(auto [_, n] : m_notes)
      n->setSelected(false);
  });

  connect(m_view, &View::dropReceived, this, &Presenter::on_drop);

  connect(m_view, &View::transposeRequested, this, &Presenter::on_transpose);

  connect(m_view, &View::deleteRequested, this, [&] {
    CommandDispatcher<>{context().context.commandStack}.submit(
        new RemoveNotes{this->model(), selectedNotes()});
  });

  for(auto& note : model.notes)
  {
    on_noteAdded(note);
  }
}

Presenter::~Presenter() { }

void Presenter::fillContextMenu(
    QMenu& menu, QPoint pos, QPointF scenepos,
    const Process::LayerContextMenuManager& cm)
{
  auto act = menu.addAction(tr("Rescale midi"));
  connect(act, &QAction::triggered, this, [&] {
    bool ok = true;
    double val = QInputDialog::getDouble(
        qApp->activeWindow(), tr("Rescale factor"), tr("Rescale factor"), 1.0, 0.0001,
        100., 8, &ok);
    if(!ok)
      return;

    CommandDispatcher<> c{context().context.commandStack};
    c.submit<RescaleMidi>(model(), val);
  });

  auto act2 = menu.addAction(tr("Rescale all midi"));
  connect(act2, &QAction::triggered, this, [&] {
    bool ok = true;
    double val = QInputDialog::getDouble(
        qApp->activeWindow(), tr("Rescale factor"), tr("Rescale factor"), 1.0, 0.0001,
        100., 8, &ok);
    if(!ok)
      return;

    MacroCommandDispatcher<RescaleAllMidi> disp{context().context.commandStack};
    auto& doc = context().context.document.model();
    auto midi = doc.findChildren<Midi::ProcessModel*>();
    for(auto ptr : midi)
    {
      disp.submit(new RescaleMidi{*ptr, val});
    }
    disp.commit();
  });
}

void Presenter::setWidth(qreal val, qreal defaultWidth)
{
  m_view->setWidth(val);
  m_view->setDefaultWidth(defaultWidth);
  for(auto [_, note] : m_notes)
    updateNote(*note);
}

void Presenter::setHeight(qreal val)
{
  m_view->setHeight(val);
  for(auto [_, note] : m_notes)
    updateNote(*note);
}

void Presenter::putToFront()
{
  m_view->setEnabled(true);
}

void Presenter::putBehind()
{
  m_view->setEnabled(false);
}

void Presenter::on_zoomRatioChanged(ZoomRatio zr)
{
  m_zr = zr;
  m_view->setDefaultWidth(model().duration().toPixels(m_zr));
  for(auto [_, note] : m_notes)
    updateNote(*note);
}

void Presenter::parentGeometryChanged() { }

const Midi::ProcessModel& Presenter::model() const noexcept
{
  return static_cast<const Midi::ProcessModel&>(m_process);
}

const Midi::View& Presenter::view() const noexcept
{
  return *m_view;
}

void Presenter::on_deselectOtherNotes()
{
  for(auto [_, n] : m_notes)
    n->setSelected(false);
}

void Presenter::on_noteChanged(NoteView& v, int semitones)
{
  auto notes = selectedNotes();
  if(ossia::find(notes, v.note.id()) == notes.end())
    notes = {v.note.id()};

  if(!m_origMovePitches)
  {
    int lo = 127, hi = 0;
    for(auto& id : notes)
    {
      const int p = model().notes.at(id).pitch();
      lo = std::min(lo, p);
      hi = std::max(hi, p);
    }
    m_origMovePitches = std::pair{lo, hi};
    m_origMoveStart = v.note.start();
  }

  // Past the range, the range grows (MoveNotes does it); past MIDI's own, the
  // selection stops as a block rather than squashing against the edge.
  const auto [lo, hi] = *m_origMovePitches;
  semitones = std::clamp(semitones, -lo, 127 - hi);

  m_moveDispatcher.submit(
      model(), notes, semitones, v.pos().x() / m_view->defaultWidth() - *m_origMoveStart);
}

void Presenter::on_noteChangeFinished(NoteView& v, int semitones)
{
  on_noteChanged(v, semitones);
  m_moveDispatcher.commit();

  m_origMovePitches = std::nullopt;
  m_origMoveStart = std::nullopt;
}

void Presenter::on_noteScaled(const Note& note, double newScale)
{
  auto notes = selectedNotes();
  auto it = ossia::find(notes, note.id());
  if(it == notes.end())
  {
    notes = {note.id()};
  }

  auto dt = newScale - note.duration();
  CommandDispatcher<>{context().context.commandStack}.submit(
      new ScaleNotes{model(), notes, dt});
}

void Presenter::on_focusChanged()
{
  if(m_keyWatched)
    m_keyWatched->removeEventFilter(this);
  m_keyWatched = nullptr;
  if(!focused())
    return;
  if(auto v = getView(*m_view))
  {
    v->installEventFilter(this);
    m_keyWatched = v;
  }
}

bool Presenter::eventFilter(QObject* watched, QEvent* event)
{
  if(event->type() != QEvent::KeyPress || !m_view->isVisible())
    return false;
  auto& ev = static_cast<QKeyEvent&>(*event);
  int dir = 0;
  if(ev.key() == Qt::Key_Up)
    dir = 1;
  else if(ev.key() == Qt::Key_Down)
    dir = -1;
  if(dir == 0 || m_selectedNotes.empty())
    return false;
  on_transpose(dir * ((ev.modifiers() & Qt::ShiftModifier) ? 12 : 1));
  ev.accept();
  return true;
}

void Presenter::on_transpose(int semitones)
{
  auto notes = selectedNotes();
  if(notes.empty() || semitones == 0)
    return;

  // The selection moves as a block: past the range, the range grows (MoveNotes
  // does it); a step that would take a note out of MIDI's 0 - 127 is refused
  // rather than squashing a chord at the edge.
  for(auto& id : notes)
  {
    const int p = model().notes.at(id).pitch() + semitones;
    if(p < 0 || p > 127)
      return;
  }

  CommandDispatcher<>{context().context.commandStack}.submit(
      new MoveNotes{model(), notes, semitones, 0.});
}

void Presenter::on_requestVelocityChange(const Note& note, double velocityDelta)
{
  auto notes = selectedNotes();
  auto it = ossia::find(notes, note.id());
  if(it == notes.end())
  {
    notes = {note.id()};
  }

  m_velocityDispatcher.submit(model(), notes, velocityDelta / 5.);
}

void Presenter::on_duplicate() { }

void Presenter::on_velocityChangeFinished()
{
  m_velocityDispatcher.commit();
}

void Presenter::on_noteSelectionChanged(NoteView* v, bool ok)
{
  if(ok)
    m_selectedNotes.insert(v);
  else
    m_selectedNotes.erase(v);

  // A rubber band or a select-all changes every note at once: the document
  // selection is pushed once for all of them.
  if(!std::exchange(m_selectionPushPending, true))
    QMetaObject::invokeMethod(
        this, &Presenter::pushNoteSelection, Qt::QueuedConnection);
}

void Presenter::pushNoteSelection()
{
  m_selectionPushPending = false;
  // m_selectedNotes holds each note once: the range constructor skips the
  // linear duplicate check of Selection::append.
  const auto notes = m_selectedNotes | boost::adaptors::transformed([](NoteView* v) {
    return static_cast<IdentifiedObjectAbstract*>(const_cast<Note*>(&v->note));
  });
  context().context.selectionStack.pushNewSelection(
      Selection(notes.begin(), notes.end()));
}

void Presenter::updateNote(NoteView& v)
{
  const auto noteRect = v.computeRect();
  const auto newPos = noteRect.topLeft();
  if(newPos != v.pos())
  {
    v.setPos(newPos);
  }

  v.setWidth(noteRect.width());
  v.setHeight(noteRect.height());
}

void Presenter::on_noteAdded(const Note& n)
{
  auto v = new NoteView{n, *this, m_view};
  updateNote(*v);
  m_notes.emplace(&n, v);
}

void Presenter::on_noteRemoving(const Note& n)
{
  if(auto it = m_notes.find(&n); it != m_notes.end())
  {
    m_selectedNotes.erase(it->second);
    delete it->second;
    m_notes.erase(it);
  }
}

void Presenter::on_notesReplaced()
{
  m_selectedNotes.clear();
  for(auto [_, n] : m_notes)
    delete n;
  m_notes.clear();

  for(auto& note : this->model().notes)
  {
    on_noteAdded(note);
  }
}

void Presenter::on_drop(const QPointF& pos, const QMimeData& md)
{
  auto songs = Midi::MidiTrack::parse(md, context().context);
  if(songs.empty())
    return;
  auto& song = songs.front();
  if(song.tracks.empty())
    return;

  auto& track = song.tracks[0];

  CommandDispatcher<> disp{m_context.context.commandStack};
  // Scale notes so that the durations are relative to the ratio of the song
  // duration & constraint duration
  const double ratio = song.durationInMs / model().duration().msec();
  for(auto& note : track.notes)
  {
    note.setStart(ratio * note.start());
    note.setDuration(ratio * note.duration());
  }
  disp.submit<Midi::ReplaceNotes>(
      model(), track.notes, track.min, track.max, model().duration());
}

std::vector<Id<Note>> Presenter::selectedNotes() const
{
  std::vector<Id<Note>> res;
  res.reserve(m_selectedNotes.size());
  for(NoteView* v : m_selectedNotes)
    res.push_back(v->note.id());
  return res;
}
}
