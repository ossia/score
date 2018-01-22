// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MidiExecutor.hpp"
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/network/midi/detail/midi_impl.hpp>
#include <Device/Protocol/DeviceList.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>
#include <Engine/score2OSSIA.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Midi/MidiProcess.hpp>

#include <ossia/dataflow/node_process.hpp>

namespace Midi
{
namespace Executor
{
class midi_node final
    : public ossia::graph_node
{
  public:
    using note_set = boost::container::flat_multiset<NoteData, NoteComparator>;
    midi_node()
    {
      m_outlets.push_back(ossia::make_outlet<ossia::midi_port>());
      m_notes.reserve(128);
      m_orig_notes.reserve(128);
      m_playingnotes.reserve(128);
      m_toStop.reserve(64);
    }

    ~midi_node() override
    {

    }

    void set_channel(int c) { m_channel = c; }

    void add_note(NoteData nd)
    {
      m_orig_notes.insert(nd);
      if(nd.start() > m_lastPos)
      {
        m_notes.insert(nd);
      }
    }

    void remove_note(NoteData nd)
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

    void update_note(NoteData oldNote, NoteData newNote)
    {
      // OPTIMIZEME
      remove_note(oldNote);
      add_note(newNote);
    }

    void set_notes(note_set&& notes)
    {
      m_notes = std::move(notes);
      m_orig_notes = m_notes;

      auto max_it = std::lower_bound(m_notes.begin(), m_notes.end(), m_lastPos, NoteComparator{});
      m_notes.erase(m_notes.begin(), max_it);
    }

    bool mustStop{};
  private:
    void run(ossia::token_request t, ossia::execution_state& e) override
    {
      auto& outlet = *m_outlets[0];
      ossia::midi_port* mp = outlet.data.target<ossia::midi_port>();

      if(mustStop)
      {
        for(auto& note : m_playingnotes)
          mp->messages.push_back(mm::MakeNoteOff(m_channel, note.pitch(), note.velocity()));

        m_notes = m_orig_notes;
        m_playingnotes.clear();

        mustStop = false;
      }
      else
      {
        if (t.date != m_prev_date)
        {
          if (m_prev_date > t.date)
            m_prev_date = 0;

          auto diff = norm(t.date, m_prev_date) / (t.date / t.position);
          m_prev_date = t.date;
          auto cur_pos = t.position;
          auto max_pos = cur_pos + diff;

          // Look for all the messages
          auto max_it = std::lower_bound(m_notes.begin(), m_notes.end(), max_pos + 1, NoteComparator{});
          for(auto it = m_notes.begin(); it < max_it; )
          {
            NoteData& note = *it;
            auto start_time = note.start();
            if (start_time >= cur_pos && start_time < max_pos)
            {
              // Send note_on
              mp->messages.push_back(mm::MakeNoteOn(m_channel, note.pitch(), note.velocity()));

              m_playingnotes.insert(note);
              it = m_notes.erase(it);
              max_it = std::lower_bound(it, m_notes.end(), max_pos + 1, NoteComparator{});
            }
            else
            {
              ++it;
            }
          }

          for(auto it = m_playingnotes.begin(); it != m_playingnotes.end(); )
          {
            NoteData& note = *it;
            auto end_time = note.end();

            if (end_time >= cur_pos && end_time < max_pos)
            {
              mp->messages.push_back(mm::MakeNoteOff(m_channel, note.pitch(), note.velocity()));

              it = m_playingnotes.erase(it);
            }
            else
            {
              ++it;
            }
          }
        }
      }

      for(const NoteData& note : m_toStop)
      {
        mp->messages.push_back(mm::MakeNoteOff(m_channel, note.pitch(), note.velocity()));
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
                     [] (const Note& n) { return n.noteData(); });

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
            this, [&,midi,cur=note.noteData()] () mutable {
      auto old = cur;
      cur = note.noteData();
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
  in_exec([nd=n.noteData(), midi] {
    midi->add_note(nd);
  });

  QObject::connect(&n, &Note::noteChanged,
          this, [&,midi,cur=n.noteData()] () mutable {
    auto old = cur;
    cur = n.noteData();
    in_exec([old, cur, midi] {
      midi->update_note(old, cur);
    });
  });
}

void Component::on_noteRemoved(const Note& n)
{
  auto midi = std::dynamic_pointer_cast<midi_node>(node);
  in_exec([nd=n.noteData(), midi] {
    midi->remove_note(nd);
  });
}


}
}
