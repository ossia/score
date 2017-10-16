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
class midi_node
    : public ossia::graph_node
{
  public:
    midi_node()
    {
      m_outlets.push_back(ossia::make_outlet<ossia::midi_port>());
    }

    ~midi_node() override
    {

    }

    void set_channel(int c) { m_channel = c; }

    void set_notes(const score::EntityMap<Note>& notes)
    {
      m_notes.clear();
      m_notes.reserve(notes.size());
      ossia::transform(notes, std::inserter(m_notes, m_notes.begin()),
                       [] (const Note& n) { return n.noteData(); });
    }

  private:
    void run(ossia::execution_state& e) override
    {
      auto& outlet = *m_outlets[0];
      ossia::midi_port* mp = outlet.data.target<ossia::midi_port>();

      if (m_date != m_prev_date)
      {
        if (m_prev_date > m_date)
          m_prev_date = 0;

        auto diff = (m_date - m_prev_date) / (m_date / m_position);
        m_prev_date = m_date;
        auto cur_pos = m_position;
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
            m_playing.insert(note.pitch());
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
            m_playing.erase(note.pitch());
          }
          else
          {
            ++it;
          }
        }
      }
    }

    using note_set = boost::container::flat_multiset<NoteData, NoteComparator>;
    note_set m_notes;
    note_set m_playingnotes;

    int m_channel{};
    boost::container::flat_set<int> m_playing;
};

class midi_node_process : public ossia::node_process
{
  public:
    void stop() override
    {
      //TODO stop all note on
    }

};
Component::Component(
    Engine::Execution::IntervalComponent& parentInterval,
    Midi::ProcessModel& element,
    const Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
  : ::Engine::Execution::
      ProcessComponent_T<Midi::ProcessModel, ossia::node_process>{
        parentInterval, element, ctx, id, "MidiComponent", parent}
{
  auto node = std::make_shared<midi_node>();
  auto proc = std::make_shared<ossia::node_process>(ctx.plugin.execGraph, node);
  m_ossia_process = proc;
  m_node = node;
  ctx.plugin.nodes.insert({element.outlet.get(), m_node});
  ctx.plugin.execGraph->add_node(m_node);

  node->set_channel(element.channel());
  node->set_notes(element.notes);
}

Component::~Component()
{
  m_node->clear();
  system().plugin.execGraph->remove_node(m_node);
}
}
}
