#include "MidiDeviceProtocol.hpp"

#include <Protocols/MIDIDevices/MidiPortResolve.hpp>

#include <ossia/detail/hash_map.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/name_validation.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/common/parameter_properties.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QDebug>

#include <atomic>

#include <libremidi/libremidi.hpp>
#include <libremidi/message.hpp>

#include <algorithm>
#include <stdexcept>

namespace Protocols::MIDIDevices
{
namespace
{
using Protocols::MIDIPorts::apiName;
using Protocols::MIDIPorts::describePort;
using Protocols::MIDIPorts::resolvePort;

//! The CC numbers that carry an NRPN, an RPN and a bank select.
enum : int
{
  cc_bank_msb = 0,
  cc_bank_lsb = 32,
  cc_data_msb = 6,
  cc_data_lsb = 38,
  cc_rpn_lsb = 100,
  cc_rpn_msb = 101,
  cc_nrpn_lsb = 98,
  cc_nrpn_msb = 99
};

/**
 * What one control listens to and how it is driven.
 *
 * A relative encoder keeps its own value here rather than in the parameter: it
 * sends a delta, so there is nothing on the wire to read the position from, and
 * the node has to hold what the deltas have added up to.
 *
 * Two threads meet in a binding: the input port's callback, which receives,
 * and whichever thread pushes the node, which sends. Only @ref position is
 * used by both, so only it is atomic; the rest belongs to one side. Pushes
 * are not concurrent with one another: the output port could not take them
 * if they were.
 */
struct binding
{
  ossia::net::parameter_base* param{};

  //! Names the values of @ref param, when they are all named. @see uniqueLabels
  ossia::net::parameter_base* choice{};

  const Control* control{};

  int lo{};
  int hi{};

  //! Where the device is believed to be. The wire and the writer both move
  //! it, each starting from wherever the other left it.
  std::atomic<int> position{};

  //! The high half of a 14-bit value, held until its low half arrives. Input
  //! callback only: both halves come through the same port.
  int pendingMsb{-1};

  //! The note a single-note control is currently sounding, or -1. Sender only.
  int sounding{-1};

  std::vector<std::pair<std::string, int>> labels;
};

//! Find or make a child. A group is shared by every control that names it, so
//! unlike a control node it must not be uniquified into `Filter.1`.
ossia::net::node_base*
findOrCreateChild(ossia::net::node_base& parent, const std::string& name)
{
  auto sanitized = name;
  ossia::net::sanitize_name(sanitized);
  if(auto* existing = parent.find_child(sanitized))
    return existing;
  return parent.create_child(sanitized);
}

ossia::net::node_base&
groupNode(ossia::net::node_base& root, const std::vector<std::string>& group)
{
  auto* node = &root;
  for(const auto& level : group)
    if(auto* child = findOrCreateChild(*node, level))
      node = child;
  return *node;
}

/**
 * The labels of a control whose values are all named, as a choice.
 *
 * Only when every label names one value: a label spanning a range has no single
 * value to send back, and a partially named set would offer the user a choice
 * that silently cannot reach half the control.
 */
std::vector<std::pair<std::string, int>> uniqueLabels(const Value& v)
{
  if(v.labels.size() < 2)
    return {};

  std::vector<std::pair<std::string, int>> out;
  for(const auto& l : v.labels)
  {
    if(l.from != l.to)
      return {};
    if(std::any_of(
           out.begin(), out.end(), [&](const auto& p) { return p.first == l.name; }))
      return {};
    out.emplace_back(l.name, l.from);
  }
  return out;
}

//! Anything addressed or sent has to fit in a data byte: a value with the top
//! bit set would be read as the start of another message.
constexpr bool isDataByte(int v) noexcept
{
  return v >= 0 && v <= 127;
}

//! 14-bit values travel as two 7-bit halves, high first.
constexpr int msb_of(int v) noexcept
{
  return (v >> 7) & 0x7F;
}
constexpr int lsb_of(int v) noexcept
{
  return v & 0x7F;
}

bool is14Bit(const Control& c) noexcept
{
  switch(c.message.type)
  {
    case MessageType::CC14:
    case MessageType::PitchBend:
      return true;
    case MessageType::NRPN:
    case MessageType::RPN:
      return resolvedRange(c.value, c.message.type).second > 127;
    default:
      return false;
  }
}

//! "-64..63 semitones", "Left..Right" -- what the source says the numbers mean.
std::string describeScale(const Scale& sc)
{
  if(sc.empty())
    return {};

  const auto number = [](double d) {
    auto s = std::to_string(d);
    s.erase(s.find_last_not_of('0') + 1);
    if(!s.empty() && s.back() == '.')
      s.pop_back();
    return s;
  };

  std::string s;
  if(sc.min && sc.max)
    s = number(*sc.min) + ".." + number(*sc.max);
  else if(!sc.from.empty() && !sc.to.empty())
    s = sc.from + ".." + sc.to;

  if(!sc.unit.empty())
    s += " " + sc.unit;
  return s;
}

std::string describe(const Control& c)
{
  std::string s = c.description;
  if(const auto scale = describeScale(c.value.scale); !scale.empty())
  {
    s += s.empty() ? "" : " - ";
    s += scale;
  }
  if(!c.value.sends.empty())
  {
    s += s.empty() ? "" : " - ";
    s += "sends";
    for(int v : c.value.sends)
      s += " " + std::to_string(v);
  }
  if(c.value.pickup)
    s += (s.empty() ? "" : " - ") + std::string{"needs soft takeover"};
  return s;
}

struct midi_device_protocol final : public ossia::net::protocol_base
{
  explicit midi_device_protocol(ProtocolSettings settings)
      : protocol_base{flags{}}
      , m_settings{std::move(settings)}
  {
    if(!m_settings.input && !m_settings.output)
      throw std::runtime_error("no MIDI port to talk to");

    m_settings.channel = std::clamp(m_settings.channel, 1, 16);

    // Every transport group: a port the resolver cannot see is one it would
    // decide had disappeared.
    libremidi::observer_configuration obsConf;
    obsConf.track_hardware = true;
    obsConf.track_virtual = true;
    obsConf.track_network = true;
    libremidi::observer obs{
        obsConf, libremidi::observer_configuration_for(m_settings.api)};

    // Resolved before either backend is constructed: a backend may start
    // processing from its constructor, so nothing should be built for a port
    // that cannot be opened.
    std::optional<libremidi::output_port> out;
    std::optional<libremidi::input_port> in;

    if(m_settings.output)
    {
      out = resolvePort(*m_settings.output, m_settings.api, obs.get_output_ports());
      if(!out)
        throw std::runtime_error{
            "MIDI output port '" + describePort(*m_settings.output)
            + "' is not available on the " + apiName(m_settings.api) + " backend"};
    }

    if(m_settings.input)
    {
      in = resolvePort(*m_settings.input, m_settings.api, obs.get_input_ports());
      if(!in)
        throw std::runtime_error{
            "MIDI input port '" + describePort(*m_settings.input)
            + "' is not available on the " + apiName(m_settings.api) + " backend"};
    }

    if(out)
    {
      libremidi::output_configuration conf{};
      m_output = std::make_unique<libremidi::midi_out>(conf, m_settings.api);
      if(m_output->open_port(*out) != stdx::error{})
        throw std::runtime_error{
            "could not open MIDI output port '" + describePort(*out) + "'"};
    }

    m_resolvedInput = in;
  }

  ~midi_device_protocol() { closePorts(); }

  /**
   * ossia::net::generic_device's destructor calls stop(), then destroys the
   * tree, then releases the protocol. The input has to be closed here: its
   * callback dispatches into the parameters that the second step frees.
   */
  void stop() override { closePorts(); }

  void closePorts() noexcept
  {
    // Input first: it is the one with a callback.
    m_input.reset();
    m_output.reset();
  }

  void set_device(ossia::net::device_base& dev) override
  {
    buildTree(dev.get_root_node());

    openInput();

    for(const auto& w : m_settings.map.warnings)
      qWarning() << "MIDI device map:" << m_settings.map.label().c_str() << ":"
                 << w.c_str();
  }

  //! Only safe once the tree its callback dispatches into is complete.
  void openInput()
  {
    if(!m_resolvedInput)
      return;

    libremidi::input_configuration conf{};
    conf.ignore_sysex = true;
    conf.on_message = [this](const libremidi::message& m) { onMessage(m); };
    m_input = std::make_unique<libremidi::midi_in>(conf, m_settings.api);
    if(m_input->open_port(*m_resolvedInput) != stdx::error{})
    {
      m_input.reset();
      throw std::runtime_error{
          "could not open MIDI input port '" + describePort(*m_resolvedInput) + "'"};
    }
  }

  /**
   * What a control may do here: what the description says it does, narrowed by
   * the ports that are actually open. A description of a controller says `in`
   * even when only an output port is configured, and a node that claims to be
   * readable from a port we never opened would never change.
   */
  ossia::access_mode accessMode(Direction d) const noexcept
  {
    const bool canSend = bool(m_settings.output);
    const bool canReceive = bool(m_settings.input);

    bool send = (d != Direction::In) && canSend;
    bool receive = (d != Direction::Out) && canReceive;

    // A control the open ports cannot reach either way still gets a node, so
    // the tree describes the device rather than the cabling -- but read-only,
    // because writing it would put a message on the cable that the device
    // never said it would listen to.
    if(!send && !receive)
      return ossia::access_mode::GET;
    if(send && receive)
      return ossia::access_mode::BI;
    return send ? ossia::access_mode::SET : ossia::access_mode::GET;
  }

  //! 1-16. A description that states several means the control exists on each;
  //! we drive the first and accept any of them.
  int sendChannel(const Message& m) const noexcept
  {
    return m.channels.empty() ? m_settings.channel : m.channels.front();
  }

  void buildTree(ossia::net::node_base& root)
  {
    m_bindings.reserve(m_settings.map.controls.size());

    for(const auto& c : m_settings.map.controls)
      addControl(root, c);
  }

  void addControl(ossia::net::node_base& root, const Control& c)
  {
    auto& parent = groupNode(root, c.group);
    auto* node = parent.create_child(c.name);
    if(!node)
      return;

    auto* param = node->create_parameter(ossia::val_type::INT);
    if(!param)
      return;

    const auto [lo, hi] = resolvedRange(c.value, c.message.type);

    m_bindings.push_back(std::make_unique<binding>());
    auto& b = *m_bindings.back();
    b.param = param;
    b.control = &c;
    b.lo = lo;
    b.hi = hi;
    // Rounded up, so a bipolar 14-bit control starts at 8192 and a 7-bit one
    // at 64: the values those devices call centre.
    b.position = c.value.def.value_or(c.value.bipolar ? (lo + hi + 1) / 2 : lo);

    param->set_access(accessMode(c.direction));
    param->set_domain(ossia::make_domain(lo, hi));
    param->set_bounding(ossia::bounding_mode::CLIP);

    if(const auto text = describe(c); !text.empty())
      ossia::net::set_description(*node, text);

    b.labels = uniqueLabels(c.value);
    if(!b.labels.empty())
      addChoice(*node, b);

    // The value the device is assumed to be at until it says otherwise; a
    // relative encoder has no other way of ever having one.
    param->set_value(b.position.load());

    m_bindingOf[param] = &b;
  }

  //! A string node beside the numeric one, naming the values it can take.
  void addChoice(ossia::net::node_base& node, binding& b)
  {
    auto* choiceNode = node.create_child("choice");
    if(!choiceNode)
      return;

    auto* choice = choiceNode->create_parameter(ossia::val_type::STRING);
    if(!choice)
      return;

    std::vector<ossia::value> names;
    names.reserve(b.labels.size());
    for(const auto& [name, _] : b.labels)
      names.emplace_back(name);

    choice->set_access(b.param->get_access());
    choice->set_domain(ossia::make_domain(names));
    choice->set_bounding(ossia::bounding_mode::CLIP);
    b.choice = choice;

    m_bindingOf[choice] = &b;
  }

  //! The label naming @p value, or the empty string when none does.
  std::string labelFor(const binding& b, int value) const
  {
    for(const auto& [name, v] : b.labels)
      if(v == value)
        return name;
    return {};
  }

  /**
   * Outbound and inbound never meet. push() is what push_value() calls once
   * the node holds its new value; set_value(), which is how the wire and a
   * control's twin update a node, never reaches it. So an incoming message
   * cannot come back out and a twin cannot send a second time, and neither
   * has to be suppressed.
   */
  bool push(const ossia::net::parameter_base& p, const ossia::value& v) override
  {
    const auto it = m_bindingOf.find(&p);
    if(it == m_bindingOf.end())
      return false;
    auto& b = *it->second;

    if(&p == b.choice)
    {
      const auto name = ossia::convert<std::string>(v);
      for(const auto& [label, value] : b.labels)
      {
        if(label == name)
        {
          b.param->set_value(value);
          return send(b, value);
        }
      }
      return false;
    }

    const int value = std::clamp(ossia::convert<int>(v), b.lo, b.hi);
    if(b.choice)
      if(const auto name = labelFor(b, value); !name.empty())
        b.choice->set_value(name);
    return send(b, value);
  }

  //! Whether a message went out.
  bool send(binding& b, int value)
  {
    // Before the port check: the position follows every write, so that a
    // relative control sends the step from where the node really was.
    const int previous = b.position.exchange(value, std::memory_order_relaxed);

    if(!m_output)
      return false;

    const auto& msg = b.control->message;
    const int ch = sendChannel(msg);

    using ce = libremidi::channel_events;

    /*
     * A relative control has no position on the wire: the device accumulates
     * what it is sent. Sending the node's value would be read as a step of that
     * size, so what goes out is the step the node just took.
     */
    if(b.control->value.mode == ValueMode::Relative)
    {
      const int delta = value - previous;
      if(delta == 0)
        return false;
      if(msg.type != MessageType::CC)
        return false;
      write(ce::control_change(
          ch, msg.number, encodeRelative(delta, *b.control->value.encoding)));
      return true;
    }

    switch(msg.type)
    {
      case MessageType::CC:
        write(ce::control_change(ch, msg.number, lsb_of(value)));
        break;

      case MessageType::CC14:
        write(ce::control_change(ch, msg.number, msb_of(value)));
        if(msg.lsb >= 0)
          write(ce::control_change(ch, msg.lsb, lsb_of(value)));
        break;

      case MessageType::NRPN:
      case MessageType::RPN:
      {
        const bool nrpn = msg.type == MessageType::NRPN;
        write(ce::control_change(ch, nrpn ? cc_nrpn_msb : cc_rpn_msb, msg.number & 0x7F));
        write(ce::control_change(
            ch, nrpn ? cc_nrpn_lsb : cc_rpn_lsb, (msg.lsb < 0 ? 0 : msg.lsb) & 0x7F));

        // A 7-bit parameter takes the value in the data MSB alone; sending only
        // its high half would write zero for everything below 128.
        if(is14Bit(*b.control))
        {
          write(ce::control_change(ch, cc_data_msb, msb_of(value)));
          write(ce::control_change(ch, cc_data_lsb, lsb_of(value)));
        }
        else
        {
          write(ce::control_change(ch, cc_data_msb, lsb_of(value)));
        }
        break;
      }

      case MessageType::Note:
        sendNote(b, ch, value);
        break;

      case MessageType::Program:
        if(msg.bank.msb >= 0)
          write(ce::control_change(ch, cc_bank_msb, msg.bank.msb & 0x7F));
        if(msg.bank.lsb >= 0)
          write(ce::control_change(ch, cc_bank_lsb, msg.bank.lsb & 0x7F));
        write(ce::program_change(ch, lsb_of(value)));
        break;

      case MessageType::PitchBend:
        write(ce::pitch_bend(ch, lsb_of(value), msb_of(value)));
        break;

      case MessageType::Aftertouch:
        write(ce::aftertouch(ch, lsb_of(value)));
        break;

      case MessageType::PolyAftertouch:
        write(ce::poly_pressure(ch, msg.number, lsb_of(value)));
        break;
    }
    return true;
  }

  /**
   * A note control's value is its velocity when it addresses one note, and the
   * note itself when it covers a range.
   *
   * Either way the previously sounding note has to be released by number: the
   * note that has to go off is the one that went on, not the one the node holds
   * now.
   */
  void sendNote(binding& b, int ch, int value)
  {
    const auto& msg = b.control->message;
    using ce = libremidi::channel_events;

    if(msg.hasRange())
    {
      if(b.sounding >= 0)
        write(ce::note_off(ch, b.sounding, 0));
      b.sounding = std::clamp(value, msg.rangeFrom, msg.rangeTo);
      write(ce::note_on(ch, b.sounding, 100));
      return;
    }

    if(value > 0)
    {
      if(b.sounding >= 0)
        write(ce::note_off(ch, b.sounding, 0));
      b.sounding = msg.number;
      write(ce::note_on(ch, msg.number, lsb_of(value)));
    }
    else if(b.sounding >= 0)
    {
      write(ce::note_off(ch, b.sounding, 0));
      b.sounding = -1;
    }
  }

  void write(const libremidi::message& m)
  {
    if(m_output)
      m_output->send_message(m);
  }

  void onMessage(const libremidi::message& m)
  {
    if(m.size() < 2)
      return;

    const int status = m.get_message_type() == libremidi::message_type::INVALID
                           ? 0
                           : (m.bytes[0] & 0xF0);
    const int channel = (m.bytes[0] & 0x0F) + 1;
    const int d1 = m.bytes[1] & 0x7F;
    const int d2 = m.size() > 2 ? (m.bytes[2] & 0x7F) : 0;

    for(auto& held : m_bindings)
    {
      auto& b = *held;
      const auto& msg = b.control->message;
      if(!acceptsChannel(msg, channel))
        continue;

      switch(msg.type)
      {
        case MessageType::CC:
          if(status == 0xB0 && d1 == msg.number)
            receive(b, d2);
          break;

        case MessageType::CC14:
          if(status != 0xB0)
            break;
          if(d1 == msg.number)
            b.pendingMsb = d2;
          else if(d1 == msg.lsb && b.pendingMsb >= 0)
          {
            receive(b, (b.pendingMsb << 7) | d2);
            b.pendingMsb = -1;
          }
          break;

        case MessageType::Note:
        {
          // The note is the address here, exactly as the CC number is above.
          const bool addressed = msg.hasRange()
                                     ? (d1 >= msg.rangeFrom && d1 <= msg.rangeTo)
                                     : (d1 == msg.number);
          if(!addressed)
            break;

          if(status == 0x90 && d2 > 0)
            receive(b, msg.hasRange() ? d1 : d2);
          else if((status == 0x80 || (status == 0x90 && d2 == 0)) && !msg.hasRange())
            receive(b, 0);
          break;
        }

        case MessageType::Program:
          if(status == 0xC0)
            receive(b, d1);
          break;

        case MessageType::PitchBend:
          if(status == 0xE0)
            receive(b, (d2 << 7) | d1);
          break;

        case MessageType::Aftertouch:
          if(status == 0xD0)
            receive(b, d1);
          break;

        case MessageType::PolyAftertouch:
          if(status == 0xA0 && d1 == msg.number)
            receive(b, d2);
          break;

        case MessageType::NRPN:
        case MessageType::RPN:
          // The parameter is selected by four separate control changes, which
          // this reader does not yet follow.
          break;
      }
    }
  }

  static bool acceptsChannel(const Message& m, int channel) noexcept
  {
    if(m.channels.empty())
      return true;
    return std::find(m.channels.begin(), m.channels.end(), channel) != m.channels.end();
  }

  /**
   * An incoming value updates the node without being sent back out: set_value
   * notifies listeners, push_value would also echo the message to the device.
   */
  void receive(binding& b, int wire)
  {
    int value = wire;

    if(b.control->value.mode == ValueMode::Relative)
    {
      const auto e = b.control->value.encoding.value_or(Encoding::TwosComplement);
      const int delta = decodeRelative(wire, e);

      // A step is applied to the position as it is at that moment, even if a
      // write moved it since the load: neither side's move is lost.
      int current = b.position.load(std::memory_order_relaxed);
      do
        value = std::clamp(current + delta, b.lo, b.hi);
      while(!b.position.compare_exchange_weak(
          current, value, std::memory_order_relaxed));
    }
    else
    {
      b.position.store(value, std::memory_order_relaxed);
    }

    b.param->set_value(value);
    if(b.choice)
      if(const auto name = labelFor(b, value); !name.empty())
        b.choice->set_value(name);
  }

  bool pull(ossia::net::parameter_base&) override { return false; }
  void request(ossia::net::parameter_base&) override { }
  std::future<void> pull_async(ossia::net::parameter_base&) override { return {}; }

  bool push_raw(const ossia::net::full_parameter_data&) override { return false; }
  bool observe(ossia::net::parameter_base&, bool) override { return true; }
  bool update(ossia::net::node_base&) override { return false; }

  ProtocolSettings m_settings;

  std::optional<libremidi::input_port> m_resolvedInput;
  std::unique_ptr<libremidi::midi_in> m_input;
  std::unique_ptr<libremidi::midi_out> m_output;

  //! Stable addresses: m_bindingOf points into them.
  std::vector<std::unique_ptr<binding>> m_bindings;

  //! A control's numeric node and its choice both resolve to the one binding;
  //! push() tells them apart by identity.
  ossia::hash_map<const ossia::net::parameter_base*, binding*> m_bindingOf;
};
}

std::unique_ptr<ossia::net::protocol_base> makeProtocol(ProtocolSettings settings)
{
  return std::make_unique<midi_device_protocol>(std::move(settings));
}

}
