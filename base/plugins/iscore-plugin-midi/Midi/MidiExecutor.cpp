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
  using error_t
      = Engine::Execution::InvalidProcessException<Midi::ProcessModel>;
  // Load the address
  // Look for the real node in the device
  auto dev_p = devices.findDevice(proc.device());
  if (!dev_p)
    throw error_t("Bad device");

  auto dev = dynamic_cast<Engine::Network::OSSIADevice*>(dev_p);
  if (!dev)
    throw error_t("Bad process");

  // We get the node corresponding to the channel
  auto node = Engine::iscore_to_ossia::findNodeFromPath(
      {QString::number(proc.channel())}, *dev->getDevice());

  m_channelNode = dynamic_cast<ossia::net::midi::channel_node*>(node);

  if (!m_channelNode)
    throw error_t("Bad node");
}

ProcessExecutor::~ProcessExecutor()
{
}

ossia::state_element ProcessExecutor::state()
{
  return state(parent()->getDate() / parent()->getDurationNominal());
}

ossia::state_element ProcessExecutor::state(double t)
{
  ossia::time_constraint& par_cst = *parent();

  ossia::time_value date = par_cst.getDate();
  if (date != mLastDate)
  {
    ossia::state st;
    if (mLastDate > date)
      mLastDate = 0;
    auto diff = (date - mLastDate) / par_cst.getDurationNominal();
    mLastDate = date;
    auto cur_pos = t;
    auto max_pos = cur_pos + diff;

    // Look for all the messages
    // TODO do something more intelligent, by
    // sorting them and only taking the current & next ones for instance
    for (Note& note : m_process.notes)
    {
      auto start_time = note.start();
      auto end_time = note.end();

      if (start_time >= cur_pos && start_time < max_pos)
      {
        // Send note_on
        auto p = m_channelNode->note_on(note.pitch(), note.velocity());
        st.add(std::move(p[0]));
        st.add(std::move(p[1]));

        m_playing.insert(note.pitch());
      }
      if (end_time >= cur_pos && end_time < max_pos)
      {
        // Send note_on
        auto p = m_channelNode->note_off(note.pitch(), note.velocity());
        st.add(std::move(p[0]));
        st.add(std::move(p[1]));

        m_playing.erase(note.pitch());
      }
    }

    m_lastState = std::move(st);
  }

  if(unmuted())
    return m_lastState;
  return {};
}

ossia::state_element ProcessExecutor::offset(ossia::time_value off)
{
  return state(off / parent()->getDurationNominal());
}

void ProcessExecutor::stop()
{
  ossia::state s;
  for (auto& note : m_playing)
  {
    auto p = m_channelNode->note_off(note, 0);
    s.add(std::move(p[0]));
    s.add(std::move(p[1]));
  }
  s.launch();
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
