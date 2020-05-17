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
using midi_node_process = ossia::nodes::midi_node_process;
Component::Component(
    Midi::ProcessModel& element,
    const Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : ::Execution::ProcessComponent_T<Midi::ProcessModel, ossia::node_process>{
        element,
        ctx,
        id,
        "MidiComponent",
        parent}
{
  auto midi = std::make_shared<midi_node>();
  this->node = midi;
  m_ossia_process = std::make_shared<midi_node_process>(midi);

  midi->set_channel(element.channel());
  auto set_notes = [&, midi] {
    midi_node::note_set notes;
    notes.container.reserve(element.notes.size());
    for (const auto& n : element.notes)
    {
      auto data = n.noteData();
      if (data.start() < 0 && data.end() > 0)
      {
        data.setStart(0.);
        data.setDuration(data.duration() + data.start());
      }
      notes.insert(to_note(data));
    }

    in_exec([n = std::move(notes), midi]() mutable { midi->set_notes(std::move(n)); });
  };
  set_notes();

  element.notes.added.connect<&Component::on_noteAdded>(this);
  element.notes.removing.connect<&Component::on_noteRemoved>(this);

  for (auto& note : element.notes)
  {
    QObject::connect(
        &note, &Note::noteChanged, this, [&, midi, cur = to_note(note.noteData())]() mutable {
          auto old = cur;
          cur = to_note(note.noteData());
          in_exec([old, cur, midi] { midi->update_note(old, cur); });
        });
  }
  QObject::connect(&element, &Midi::ProcessModel::notesChanged, this, set_notes);
}

Component::~Component() { }

void Component::on_noteAdded(const Note& n)
{
  auto midi = std::dynamic_pointer_cast<midi_node>(node);
  in_exec([nd = to_note(n.noteData()), midi] { midi->add_note(nd); });

  QObject::connect(&n, &Note::noteChanged, this, [&, midi, cur = to_note(n.noteData())]() mutable {
    auto old = cur;
    cur = to_note(n.noteData());
    in_exec([old, cur, midi] { midi->update_note(old, cur); });
  });
}

void Component::on_noteRemoved(const Note& n)
{
  auto midi = std::dynamic_pointer_cast<midi_node>(node);
  in_exec([nd = to_note(n.noteData()), midi] { midi->remove_note(nd); });
}

ossia::nodes::note_data Component::to_note(const NoteData& n)
{
  auto& cv_time = system().time;
  return {
      cv_time(process().duration() * n.start()),
      cv_time(process().duration() * n.duration()),
      n.pitch(),
      n.velocity()};
}
}
}
