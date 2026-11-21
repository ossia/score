// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MidiExecutor.hpp"

#include <Midi/MidiProcess.hpp>

#include <ossia/dataflow/nodes/midi.hpp>
namespace Midi
{
namespace Executor
{

using midi_node = ossia::nodes::midi;

//! A note starting before the process and ending in it plays from its start.
static NoteData played_part(NoteData data)
{
  if(data.start() < 0 && data.end() > 0)
  {
    data.setStart(0.);
    data.setDuration(data.duration() + data.start());
  }
  return data;
}

static midi_node::note_set to_ossia(Component& c)
{
  midi_node::note_set notes;
  auto& element = c.process();
  notes.reserve(element.notes.size());
  for(const auto& n : element.notes)
    notes.insert(c.to_note(n.noteData()));
  return notes;
}

using midi_node_process = ossia::nodes::midi_node_process;
Component::Component(
    Midi::ProcessModel& element, const Execution::Context& ctx, QObject* parent)
    : ::Execution::ProcessComponent_T<Midi::ProcessModel, ossia::node_process>{
        element, ctx, "MidiComponent", parent}
{
  auto midi = ossia::make_node<midi_node>(*ctx.execState, element.notes.size());
  this->node = midi;
  m_ossia_process = std::make_shared<midi_node_process>(midi);

  midi->set_channel(element.channel());
  midi->set_notes(to_ossia(*this));

  element.notes.added.connect<&Component::on_noteAdded>(this);
  element.notes.removing.connect<&Component::on_noteRemoved>(this);
  element.notes.replaced.connect<&Component::on_notesReplaced>(this);

  for(auto& note : element.notes)
    connect_note(note);

  QObject::connect(
      &element, &Midi::ProcessModel::notesChanged, this, &Component::on_notesReplaced);
  // The node's notes are dates, made from the notes' positions relative to
  // the duration of the process.
  QObject::connect(
      &element, &Midi::ProcessModel::durationChanged, this,
      &Component::on_notesReplaced);
}

void Component::connect_note(const Note& n)
{
  // One connection per note: it keeps what the node was last sent of the note,
  // which is how the node finds the note to change.
  QObject::disconnect(&n, &Note::noteChanged, this, nullptr);
  auto midi = std::dynamic_pointer_cast<midi_node>(node);
  QObject::connect(
      &n, &Note::noteChanged, this, [&n, this, midi, cur = to_note(n.noteData())]() mutable {
        auto old = cur;
        cur = to_note(n.noteData());
        in_exec([old, cur, midi] { midi->update_note(old, cur); });
      });
}

Component::~Component() { }

void Component::on_noteAdded(const Note& n)
{
  auto midi = std::dynamic_pointer_cast<midi_node>(node);
  in_exec([nd = to_note(n.noteData()), midi] { midi->add_note(nd); });
  connect_note(n);
}

void Component::on_noteRemoved(const Note& n)
{
  auto midi = std::dynamic_pointer_cast<midi_node>(node);
  in_exec([nd = to_note(n.noteData()), midi] { midi->remove_note(nd); });
}

void Component::on_notesReplaced()
{
  auto midi = std::dynamic_pointer_cast<midi_node>(node);

  in_exec([n = to_ossia(*this), midi]() mutable { midi->replace_notes(std::move(n)); });

  // The notes may be new objects, or the same ones at other dates.
  for(auto& note : process().notes)
    connect_note(note);
}
ossia::nodes::note_data Component::to_note(const NoteData& note)
{
  const auto n = played_part(note);
  auto& cv_time = system().time;
  return {
      cv_time(process().duration() * n.start()),
      cv_time(process().duration() * n.duration()), n.pitch(), n.velocity()};
}
}
}
