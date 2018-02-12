// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MidiExecutor.hpp"
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/network/midi/detail/midi_impl.hpp>
#include <Explorer/DeviceList.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>
#include <Engine/score2OSSIA.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Midi/MidiProcess.hpp>

#include <ossia/dataflow/node_process.hpp>

namespace Midi
{
namespace Executor
{
struct note_data
{
    int64_t start{};
    int64_t duration{};

    midi_size_t pitch{};
    midi_size_t velocity{};
};
struct note_comparator
{
  bool operator()(const note_data& lhs, const note_data& rhs) const
  {
    return lhs.start < rhs.start;
  }
  bool operator()(const note_data& lhs, int64_t rhs) const {
    return lhs.start < rhs;
  }
};
class midi_node final
    : public ossia::nonowning_graph_node
{
    ossia::outlet midi_out{ossia::midi_port{}};
  public:
    using note_set = boost::container::flat_multiset<note_data, note_comparator>;
    midi_node()
    {
      m_outlets.push_back(&midi_out);
      m_notes.reserve(128);
      m_orig_notes.reserve(128);
      m_playingnotes.reserve(128);
      m_toStop.reserve(64);
    }

    ~midi_node() override
    {

    }

    void set_channel(int c) { m_channel = c; }

    void add_note(note_data nd)
    {
      m_orig_notes.insert(nd);
      if(nd.start > m_lastPos)
      {
        m_notes.insert(nd);
      }
    }

    void remove_note(note_data nd)
    {
      m_orig_notes.erase(nd);
      m_notes.erase(nd);
      auto it = m_playingnotes.find(nd);
      if(it != m_playingnotes.end())
      {
        m_toStop.insert(nd);
        m_playingnotes.erase(it);
      }
    }

    void update_note(note_data oldNote, note_data newNote)
    {
      // OPTIMIZEME
      remove_note(oldNote);
      add_note(newNote);
    }

    void set_notes(note_set&& notes)
    {
      m_notes = std::move(notes);
      m_orig_notes = m_notes;

      auto max_it = std::lower_bound(m_notes.begin(), m_notes.end(), m_lastPos, note_comparator{});
      m_notes.erase(m_notes.begin(), max_it);
    }

    bool mustStop{};
  private:
    void run(ossia::token_request t, ossia::execution_state& e) override
    {
      ossia::midi_port* mp = midi_out.data.target<ossia::midi_port>();

      if(mustStop)
      {
        for(auto& note : m_playingnotes)
        {
          mp->messages.push_back(mm::MakeNoteOff(m_channel, note.pitch, note.velocity));
          mp->messages.back().timestamp = t.offset;
        }

        m_notes = m_orig_notes;
        m_playingnotes.clear();

        mustStop = false;
      }
      else
      {
        if (t.date > m_prev_date)
        {
          if (m_prev_date > t.date)
            m_prev_date = 0;

          // First send note offs
          for(auto it = m_playingnotes.begin(); it != m_playingnotes.end(); )
          {
            note_data& note = *it;
            auto end_time = note.start + note.duration;

            if (end_time >= m_prev_date && end_time < t.date)
            {
              mp->messages.push_back(mm::MakeNoteOff(m_channel, note.pitch, note.velocity));
              mp->messages.back().timestamp = t.offset + (end_time - m_prev_date);


              it = m_playingnotes.erase(it);
            }
            else
            {
              ++it;
            }
          }

          // Look for all the messages
          auto max_it = std::lower_bound(m_notes.begin(), m_notes.end(), t.date, note_comparator{});
          for(auto it = m_notes.begin(); it < max_it; )
          {
            note_data& note = *it;
            auto start_time = note.start;
            if (start_time >= m_prev_date && start_time < t.date)
            {
              // Send note_on
              mp->messages.push_back(mm::MakeNoteOn(m_channel, note.pitch, note.velocity));
              mp->messages.back().timestamp = t.offset + (start_time - m_prev_date);

              m_playingnotes.insert(note);
              it = m_notes.erase(it);
              max_it = std::lower_bound(it, m_notes.end(), t.date + int64_t{1}, note_comparator{});
            }
            else
            {
              ++it;
            }
          }


          m_prev_date = t.date;
        }
      }

      for(const note_data& note : m_toStop)
      {
        mp->messages.push_back(mm::MakeNoteOff(m_channel, note.pitch, note.velocity));
        mp->messages.back().timestamp = t.offset;
      }
      m_toStop.clear();

      m_lastPos = t.position;
    }

    note_set m_notes;
    note_set m_orig_notes;
    note_set m_playingnotes;
    note_set m_toStop;

    double m_lastPos{};
    int m_channel{};

};

class midi_node_process final : public ossia::node_process
{
  public:
    using ossia::node_process::node_process;
  void stop() override
  {
    midi_node* n = static_cast<midi_node*>(node.get());
    n->requested_tokens.push_back(ossia::token_request{});
    n->mustStop = true;
  }
};
Component::Component(
    Midi::ProcessModel& element,
    const Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
  : ::Engine::Execution::
      ProcessComponent_T<Midi::ProcessModel, ossia::node_process>{
        element, ctx, id, "MidiComponent", parent}
{
  auto midi = std::make_shared<midi_node>();
  this->node = midi;
  m_ossia_process = std::make_shared<midi_node_process>(midi);

  midi->set_channel(element.channel());
  auto set_notes = [&,midi] {
    midi_node::note_set notes;
    notes.reserve(element.notes.size());
    ossia::transform(element.notes, std::inserter(notes, notes.begin()),
                     [&] (const Note& n) {
      auto data = n.noteData();
      if(data.start() < 0 && data.end() > 0)
      {
        data.setStart(0.);
        data.setDuration(data.duration() + data.start());
      }
      return to_note(data);
    });

    for(auto& note : notes)
      std::cerr << note.start << " " << note.duration << "\n";
     in_exec([n=std::move(notes), midi] () mutable {
      midi->set_notes(std::move(n));
    });
  };
  set_notes();

  element.notes.added.connect<Component, &Component::on_noteAdded>(this);
  element.notes.removing.connect<Component, &Component::on_noteRemoved>(this);

  for(auto& note : element.notes)
  {
    QObject::connect(&note, &Note::noteChanged,
            this, [&,midi,cur=to_note(note.noteData())] () mutable {
      auto old = cur;
      cur = to_note(note.noteData());
      in_exec([old, cur, midi] {
        midi->update_note(old, cur);
      });
    });
  }
  QObject::connect(
        &element, &Midi::ProcessModel::notesChanged,
        this, set_notes);
}

Component::~Component()
{
}

void Component::on_noteAdded(const Note& n)
{
  auto midi = std::dynamic_pointer_cast<midi_node>(node);
  in_exec([nd=to_note(n.noteData()), midi] {
    midi->add_note(nd);
  });

  QObject::connect(&n, &Note::noteChanged,
          this, [&,midi,cur=to_note(n.noteData())] () mutable {
    auto old = cur;
    cur = to_note(n.noteData());
    in_exec([old, cur, midi] {
      midi->update_note(old, cur);
    });
  });
}

void Component::on_noteRemoved(const Note& n)
{
  auto midi = std::dynamic_pointer_cast<midi_node>(node);
  in_exec([nd=to_note(n.noteData()), midi] {
    midi->remove_note(nd);
  });
}

note_data Component::to_note(const NoteData& n) {
  auto& cv_time = system().time;
  return note_data{ cv_time(process().duration() * n.start()), cv_time(process().duration() * n.duration()), n.pitch(), n.velocity() };
}


}
}
