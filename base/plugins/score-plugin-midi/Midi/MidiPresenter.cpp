// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Midi/MidiNoteView.hpp>
#include <Midi/MidiPresenter.hpp>
#include <Midi/MidiProcess.hpp>
#include <Midi/MidiView.hpp>

#include <Process/Focus/FocusDispatcher.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>

#include <Midi/Commands/AddNote.hpp>
#include <Midi/Commands/RemoveNotes.hpp>
#include <Midi/Commands/ScaleNotes.hpp>

#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <ossia/detail/math.hpp>

namespace Midi
{
Presenter::Presenter(
    const Midi::ProcessModel& layer,
    View* view,
    const Process::ProcessPresenterContext& ctx,
    QObject* parent)
    : LayerPresenter{ctx, parent}
    , m_layer{layer}
    , m_view{view}
    , m_ongoing{ctx.commandStack}
    , m_zr{1.}
{
  putToFront();

  auto& model = layer;

  con(model, &ProcessModel::notesChanged, this, [&]() {

    for (auto note : m_notes)
    {
      delete note;
    }

    m_notes.clear();

    for (auto& note : model.notes)
    {
      on_noteAdded(note);
    }
  });
  model.notes.added.connect<Presenter, &Presenter::on_noteAdded>(this);
  model.notes.removing.connect<Presenter, &Presenter::on_noteRemoving>(this);

  connect(m_view, &View::doubleClicked, this, [&](QPointF pos) {
    CommandDispatcher<>{context().context.commandStack}.submitCommand(
        new AddNote{layer,
                    noteAtPos(pos, m_view->boundingRect(), m_view->defaultWidth())});
  });

  connect(m_view, &View::pressed, this, [&]() {
    m_context.context.focusDispatcher.focus(this);
  });

  connect(m_view, &View::deleteRequested, this, [&] {
    CommandDispatcher<>{context().context.commandStack}.submitCommand(
        new RemoveNotes{m_layer, selectedNotes()});

  });

  connect(
      m_view, &View::askContextMenu, this, &Presenter::contextMenuRequested);

  for (auto& note : model.notes)
  {
    on_noteAdded(note);
  }
}

void Presenter::setWidth(qreal val)
{
  m_view->setWidth(val);
  m_view->setDefaultWidth(m_layer.duration().toPixels(m_zr));
  for (auto note : m_notes)
    updateNote(*note);
}

void Presenter::setHeight(qreal val)
{
  m_view->setHeight(val);
  for (auto note : m_notes)
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
  m_view->setDefaultWidth(m_layer.duration().toPixels(m_zr));
  for (auto note : m_notes)
    updateNote(*note);
}

void Presenter::parentGeometryChanged()
{
}

const Midi::ProcessModel& Presenter::model() const
{
  return m_layer;
}

const Id<Process::ProcessModel>& Presenter::modelId() const
{
  return m_layer.id();
}

void Presenter::setupNote(NoteView& v)
{
  const auto note_height = m_view->height() / 127.;
  v.setPos(
      v.note.start() * m_view->defaultWidth(),
      m_view->height() - v.note.pitch() * note_height);
  v.setWidth(v.note.duration() * m_view->defaultWidth());
  v.setHeight(note_height);

  con(v.note, &Note::noteChanged, &v, [&] { updateNote(v); });

  con(v, &NoteView::noteChangeFinished, this, [&] {
    auto newPos = v.pos();
    auto rect = m_view->boundingRect();
    auto height = rect.height();

    // Snap to grid : we round y to the closest multiple of 127
    int note = ossia::clamp(
        int(127
            - (qMin(rect.bottom(), qMax(newPos.y(), rect.top())) / height)
                  * 127),
        0,
        127);

    m_ongoing.submitCommand(
        m_layer,
        selectedNotes(),
        note - v.note.pitch(),
        newPos.x() / m_view->defaultWidth() - v.note.start());

    m_ongoing.commit();
  });

  con(v, &NoteView::noteScaled, this, [&](double newScale) {

    auto dt = newScale - v.note.duration();
    CommandDispatcher<>{context().context.commandStack}.submitCommand(
        new ScaleNotes{m_layer, selectedNotes(), dt});
  });
}

void Presenter::updateNote(NoteView& v)
{
  const auto note_height = m_view->height() / 127.;
  QPointF newPos{v.note.start() * m_view->defaultWidth(),
                 m_view->height() - std::ceil(v.note.pitch() * note_height)};

  if (newPos != v.pos())
    v.setPos(newPos);

  v.setWidth(v.note.duration() * m_view->defaultWidth());
  v.setHeight(note_height);
}

void Presenter::on_noteAdded(const Note& n)
{
  auto v = new NoteView{n, m_view};
  setupNote(*v);
  m_notes.push_back(v);
}

void Presenter::on_noteRemoving(const Note& n)
{
  auto it = ossia::find_if(
      m_notes, [&](const auto& other) { return &other->note == &n; });
  if (it != m_notes.end())
  {
    delete *it;
    m_notes.erase(it);
  }
}

std::vector<Id<Note>> Presenter::selectedNotes() const
{
  using namespace boost::adaptors;

  std::vector<Id<Note>> res;
  boost::copy(
      m_notes | filtered([](NoteView* v) { return v->isSelected(); })
          | transformed([](NoteView* v) { return v->note.id(); }),
      std::back_inserter(res));
  return res;
}
}
