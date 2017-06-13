#include "MidiExecutor.hpp"
#include <ossia/editor/scenario/time_constraint.hpp>
#include <ossia/network/midi/detail/midi_impl.hpp>
#include <Device/Protocol/DeviceList.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Midi/MidiProcess.hpp>

namespace Midi
{
namespace Executor
{
ProcessExecutor::ProcessExecutor(
    const Midi::ProcessModel& proc, const Device::DeviceList& devices)
  : m_process{proc}
{
  if(proc.device().isEmpty())
  {
    // We're in "audio" mode.
  }
  else
  {
    // Load the address
    // Look for the real node in the device
    auto dev_p = devices.findDevice(proc.device());
    if(dev_p)
    {
      auto dev = dynamic_cast<Engine::Network::OSSIADevice*>(dev_p);
      if (dev)
      {
        // We get the node corresponding to the channel
        auto node = Engine::iscore_to_ossia::findNodeFromPath(
        {QString::number(proc.channel())}, *dev->getDevice());

        if(node)
          m_channelNode = dynamic_cast<ossia::net::midi::channel_node*>(node);
      }
    }
  }

  m_notes.reserve(m_process.notes.size());
  ossia::transform(m_process.notes, std::inserter(m_notes, m_notes.begin()),
                   [] (const Note& n) { return n.noteData(); });
}

ProcessExecutor::~ProcessExecutor()
{
}

ossia::state_element ProcessExecutor::state(ossia::time_value date, double pos)
{
  ossia::time_constraint& par_cst = *parent();

  if (date != m_lastDate)
  {
    m_lastState.clear();
    if (m_lastDate > date)
      m_lastDate = 0;

    timedState.currentAudioStart.clear();
    timedState.currentAudioStop.clear();

    auto diff = (date - m_lastDate) / par_cst.get_nominal_duration();
    m_lastDate = date;
    auto cur_pos = pos;
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
        if(m_channelNode)
        {
          auto p = m_channelNode->note_on(note.pitch(), note.velocity());
          m_lastState.add(std::move(p[0]));
          m_lastState.add(std::move(p[1]));
        }
        else
        {
          timedState.currentAudioStart.emplace_back(note, 0);
        }

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
        if(m_channelNode)
        {
          // Send note_off
          auto p = m_channelNode->note_off(note.pitch(), note.velocity());
          m_lastState.add(std::move(p[0]));
          m_lastState.add(std::move(p[1]));
        }
        else
        {
          timedState.currentAudioStop.emplace_back(note, 0);
        }

        it = m_playingnotes.erase(it);
        m_playing.erase(note.pitch());
      }
      else
      {
        ++it;
      }
    }
  }

  if(unmuted())
    return m_lastState;
  return {};
}

ossia::state_element ProcessExecutor::offset(ossia::time_value off, double pos)
{
  // TODO
  return {};
}

void ProcessExecutor::stop()
{
  if(m_channelNode)
  {
    ossia::state s;
    for (auto& note : m_playing)
    {
      auto p = m_channelNode->note_off(note, 0);
      s.add(std::move(p[0]));
      s.add(std::move(p[1]));
    }
    s.launch();
  }

  timedState.currentAudioStart.clear();
  m_playing.clear();
}

Component::Component(
    Engine::Execution::ConstraintComponent& parentConstraint,
    Midi::ProcessModel& element,
    const Engine::Execution::Context& ctx,
    const Id<iscore::Component>& id,
    QObject* parent)
  : ::Engine::Execution::
      ProcessComponent_T<Midi::ProcessModel, ProcessExecutor>{
        parentConstraint, element, ctx, id, "MidiComponent", parent}
{
  m_ossia_process = std::make_shared<ProcessExecutor>(element, ctx.devices.list());
}
}
}
