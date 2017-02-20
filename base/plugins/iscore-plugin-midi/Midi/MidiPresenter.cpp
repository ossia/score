#include <Midi/MidiLayer.hpp>
#include <Midi/MidiNoteView.hpp>
#include <Midi/MidiPresenter.hpp>
#include <Midi/MidiProcess.hpp>
#include <Midi/MidiView.hpp>

#include <Process/Focus/FocusDispatcher.hpp>

#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/document/DocumentInterface.hpp>

#include <Midi/Commands/AddNote.hpp>
#include <Midi/Commands/RemoveNotes.hpp>
#include <Midi/Commands/ScaleNotes.hpp>

#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/copy.hpp>

namespace Midi
{
Presenter::Presenter(
    const Layer& layer,
    View* view,
    const Process::ProcessPresenterContext& ctx,
    QObject* parent)
    : LayerPresenter{ctx, parent}
    , m_layer{layer}
    , m_view{view}
    , m_ongoing{ctx.commandStack}
{
  putToFront();

  auto& model = layer.processModel();

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
        new AddNote{layer.processModel(),
                    noteAtPos(pos, m_view->boundingRect())});
  });

  connect(m_view, &View::pressed, this, [&]() {
    m_context.context.focusDispatcher.focus(this);
  });

  connect(m_view, &View::deleteRequested, this, [&] {
    CommandDispatcher<>{context().context.commandStack}.submitCommand(
        new RemoveNotes{m_layer.processModel(), selectedNotes()});

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

void Presenter::on_zoomRatioChanged(ZoomRatio)
{
}

void Presenter::parentGeometryChanged()
{
}

const Process::LayerModel& Presenter::layerModel() const
{
  return m_layer;
}

const Id<Process::ProcessModel>& Presenter::modelId() const
{
  return m_layer.processModel().id();
}

void Presenter::setupNote(NoteView& v)
{
  const auto note_height = m_view->height() / 127.;
  v.setPosition({
      v.note.start() * m_view->width(),
      m_view->height() - v.note.pitch() * note_height});
  v.setWidth(v.note.duration() * m_view->width());
  v.setHeight(note_height);

  con(v.note, &Note::noteChanged, &v, [&] { updateNote(v); });

  con(v, &NoteView::noteChangeFinished, this, [&] {
    auto newPos = v.position();
    auto rect = m_view->boundingRect();
    auto height = rect.height();

    // Snap to grid : we round y to the closest multiple of 127
    int note = qBound(
        0,
        int(127
            - (qMin(rect.bottom(), qMax(newPos.y(), rect.top())) / height)
                  * 127),
        127);

    m_ongoing.submitCommand(
        m_layer.processModel(),
        selectedNotes(),
        note - v.note.pitch(),
        newPos.x() / rect.width() - v.note.start());

    m_ongoing.commit();
  });

  con(v, &NoteView::noteScaled, this, [&](double newScale) {

    auto dt = newScale - v.note.duration();
    CommandDispatcher<>{context().context.commandStack}.submitCommand(
        new ScaleNotes{m_layer.processModel(), selectedNotes(), dt});
  });
}

void Presenter::updateNote(NoteView& v)
{
  const auto note_height = m_view->height() / 127.;
  QPointF newPos{v.note.start() * m_view->width(),
                 m_view->height() - v.note.pitch() * note_height};
  if (newPos != v.position())
    v.setPosition(newPos);

  v.setWidth(v.note.duration() * m_view->width());
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
      m_notes | filtered([](NoteView* v) { return false; /* v->isSelected() */; })
          | transformed([](NoteView* v) { return v->note.id(); }),
      std::back_inserter(res));
  return res;
}
}
