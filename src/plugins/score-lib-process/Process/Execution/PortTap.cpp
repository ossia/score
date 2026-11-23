#include "PortTap.hpp"

#include <Process/Execution/TapLink.hpp>

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>
#include <Process/ExecutionTransaction.hpp>

#include <score/model/path/PathSerialization.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/value_port.hpp>
#include <ossia/network/common/destination_qualifiers.hpp>

#include <libremidi/cmidi2.hpp>

#include <algorithm>

namespace Execution
{
TapMonitor::TapMonitor(Process::PortType t)
    : type{t}
{
}
TapMonitor::~TapMonitor() = default;

namespace
{
TapTicker* g_ticker{};

//! Length of the MIDI 1 message that starts with `status`, without SysEx.
constexpr std::size_t midi1Length(uint8_t status) noexcept
{
  switch(status & 0xF0)
  {
    case 0xC0:
    case 0xD0:
      return 2;
    case 0xF0:
      return status == 0xF2 ? 3 : (status == 0xF1 || status == 0xF3) ? 2 : 1;
    default:
      return 3;
  }
}

//! The MIDI 1 messages of a UMP, MIDI 1 or MIDI 2, as libremidi converts
//! them: some MIDI 2 messages give several (a program change with its bank,
//! a registered controller), some none (per-note controllers). A SysEx is
//! shown by its status byte only.
template <typename F>
void toMidi1(const libremidi::ump& u, F&& push) noexcept
{
  uint8_t bytes[16]{};
  auto* ump = const_cast<cmidi2_ump*>(reinterpret_cast<const cmidi2_ump*>(u.data));
  const std::size_t n = cmidi2_convert_single_ump_to_midi1(bytes, sizeof(bytes), ump);

  // A MIDI 2 velocity below 1/128 is still a note on, not a note off
  if(n == 3 && (bytes[0] & 0xF0) == 0x90 && bytes[2] == 0
     && cmidi2_ump_get_message_type(ump) == CMIDI2_MESSAGE_TYPE_MIDI_2_CHANNEL)
    bytes[2] = 1;

  for(std::size_t i = 0; i < n;)
  {
    const uint8_t status = bytes[i];
    if(status < 0x80)
      break;
    TapMonitor::Midi m;
    if(status == 0xF0)
    {
      m.bytes[0] = status;
      m.size = 1;
      push(m);
      break;
    }
    const std::size_t len = std::min(midi1Length(status), n - i);
    for(std::size_t k = 0; k < len; k++)
      m.bytes[k] = bytes[i + k];
    m.size = uint8_t(len);
    push(m);
    i += len;
  }
}

class tap_node final : public ossia::graph_node
{
public:
  tap_node(std::shared_ptr<TapMonitor> m)
      : m_monitor{std::move(m)}
  {
    switch(m_monitor->type)
    {
      case Process::PortType::Audio:
        m_inlets.push_back(new ossia::audio_inlet);
        break;
      case Process::PortType::Midi:
        m_inlets.push_back(new ossia::midi_inlet);
        break;
      default:
        m_inlets.push_back(new ossia::value_inlet);
        break;
    }
  }

  std::string label() const noexcept override { return "port tap"; }

  void run(const ossia::token_request&, ossia::exec_state_facade e) noexcept override
  {
    auto& mon = *m_monitor;
    auto& inlet = *m_inlets.front();
    switch(mon.type)
    {
      case Process::PortType::Audio: {
        auto& port = *inlet.target<ossia::audio_port>();
        const int nch = int(port.channels());
        // The length all channels have
        int frames = nch > 0 ? int(port.channel(0).size()) : 0;
        for(int c = 1; c < nch; c++)
          frames = std::min(frames, int(port.channel(c).size()));
        mon.samples.set_rate(e.sampleRate());
        if(frames > 0)
          mon.samples.write(nch, frames, [&](int c) { return port.channel(c).data(); });
        break;
      }
      case Process::PortType::Midi: {
        for(const auto& u : inlet.target<ossia::midi_port>()->messages)
          toMidi1(u, [&](const TapMonitor::Midi& m) { mon.midi.push(m); });
        break;
      }
      default: {
        for(const auto& v : inlet.target<ossia::value_port>()->get_data())
          mon.values.try_enqueue(v.value);
        break;
      }
    }

    writeInPlaceOfOutlets(inlet, e);
  }

private:
  std::shared_ptr<TapMonitor> m_monitor;
};
}

TapTicker::TapTicker()
{
  g_ticker = this;
  m_nodes.reserve(8);
}

TapTicker::~TapTicker()
{
  if(g_ticker == this)
    g_ticker = nullptr;
  m_current.store(nullptr, std::memory_order_seq_cst);
}

TapTicker* TapTicker::instance() noexcept
{
  return g_ticker;
}

void TapTicker::startTick(const ossia::audio_tick_state& st)
{
  m_ticks.fetch_add(1, std::memory_order_seq_cst);
  if(const auto* list = m_current.load(std::memory_order_seq_cst))
    for(const auto& n : *list)
      if(!n->enabled())
        n->request(ossia::token_request{});
  m_ticks.fetch_add(1, std::memory_order_seq_cst);
}

void TapTicker::add(std::shared_ptr<ossia::graph_node> node)
{
  m_nodes.push_back(std::move(node));
  publish();
}

void TapTicker::remove(const ossia::graph_node* node)
{
  std::erase_if(m_nodes, [node](const auto& n) { return n.get() == node; });
  publish();
}

void TapTicker::publish()
{
  auto list = m_nodes.empty() ? nullptr : std::make_unique<const node_list>(m_nodes);
  m_current.store(list.get(), std::memory_order_seq_cst);
  if(m_published)
    m_retired.push_back({std::move(m_published), m_ticks.load(std::memory_order_seq_cst)});
  m_published = std::move(list);
  collect();
}

void TapTicker::collect()
{
  // Seen out of startTick, or since then in another one: the execution
  // thread reads the newer list
  const uint64_t now = m_ticks.load(std::memory_order_seq_cst);
  std::erase_if(m_retired, [now](const retired& r) {
    return r.ticks % 2 == 0 || r.ticks != now;
  });
}

PortTap::PortTap(const Process::Port& port, const Execution::Context& ctx)
    : m_context{ctx.weakSelf()}
{
  const auto type = port.type();
  if(type != Process::PortType::Audio && type != Process::PortType::Midi
     && type != Process::PortType::Message)
    return;
  if(!TapTicker::instance() || ctx.retired || !ctx.execGraph)
    return;

  m_monitor = std::make_shared<TapMonitor>(type);
  auto node = std::make_shared<tap_node>(m_monitor);
  m_link = std::make_unique<TapLink>(port, ctx, node);
  if(!m_link->linked())
  {
    m_link.reset();
    m_monitor.reset();
    return;
  }

  if(type == Process::PortType::Message)
  {
    // The type a node declares is set when it is built; the address unit
    // changes from the execution thread, so it is read from the model
    auto declared = [](const ossia::value_port* p) -> ossia::unit_t {
      if(p)
        if(auto* u = p->type.target<ossia::unit_t>())
          return *u;
      return {};
    };
    auto& setup = ctx.setup;
    if(auto* observed = m_link->observedInlet())
      m_monitor->unit = declared(observed->target<ossia::value_port>());
    else if(auto it = setup.outlets.find(
                const_cast<Process::Outlet*>(qobject_cast<const Process::Outlet*>(&port)));
            it != setup.outlets.end() && it->second.second)
      m_monitor->unit = declared(it->second.second->target<ossia::value_port>());
    if(!m_monitor->unit)
      m_monitor->unit = port.address().qualifiers.get().unit;
  }
}

TapMonitor* PortTap::monitor() const noexcept
{
  if(!m_monitor)
    return nullptr;
  auto ctx = m_context.lock();
  return ctx && !ctx->retired ? m_monitor.get() : nullptr;
}

PortTap::~PortTap() = default;
}
