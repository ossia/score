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
#include <ossia/protocols/midi/midi_stream.hpp>

#include <QDebug>

#include <atomic>

#include <libremidi/detail/conversion.hpp>
#include <libremidi/libremidi.hpp>
#include <libremidi/message.hpp>

#include <algorithm>
#include <array>
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
//! The parameter a channel currently has selected, which the four control
//! changes of an NRPN or RPN build up between them.
struct selection
{
  int number{-1};
  int lsb{-1};
  int data{-1};
  bool registered{};
};

struct binding
{
  ossia::net::parameter_base* param{};

  //! Names the values of @ref param, when they are all named. @see uniqueLabels
  ossia::net::parameter_base* choice{};

  const Control* control{};

  //! The channel of the description this control came from, for a control
  //! whose own message states none.
  int defaultChannel{1};

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

/**
 * One MIDI channel addressed as itself, for what no description covers.
 *
 * The nodes carry no state of their own: what a channel means is whatever the
 * device at the other end makes of it, so a message read from the wire lands
 * on the node and a value written to the node leaves as a message, and that is
 * all.
 */
struct raw_channel
{
  int channel{1};

  //! (note, velocity) and (control, value): the number is half of the value.
  ossia::net::parameter_base* on{};
  ossia::net::parameter_base* off{};
  ossia::net::parameter_base* control{};
  ossia::net::parameter_base* program{};
  ossia::net::parameter_base* pitchbend{};

  //! Empty, or 128 nodes each addressing one number.
  std::vector<ossia::net::parameter_base*> noteOn;
  std::vector<ossia::net::parameter_base*> noteOff;
  std::vector<ossia::net::parameter_base*> controls;
  std::vector<ossia::net::parameter_base*> programs;
};

//! Which message a raw node stands for, for the one push() that reaches it.
struct raw_address
{
  enum Kind
  {
    NoteOn,
    NoteOff,
    Control,
    Program,
    PitchBend
  };

  raw_channel* chan{};
  Kind kind{};

  //! The number the node addresses, or -1 when the value carries it.
  int number{-1};
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
 * The values a control names, as a choice.
 *
 * Only where every label names one value: a label spanning a range has no
 * single value to send back. The choice reaches the named values and the
 * numeric node beside it reaches the rest, so one name is worth offering --
 * a bank holding a single patch names it, and hiding that would lose the only
 * thing the document says about it.
 */
std::vector<std::pair<std::string, int>> uniqueLabels(const Value& v)
{
  if(v.labels.empty())
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

/**
 * @see ossia::net::midi::midi_stream for the second half of what this is: a
 * tree of parameters, and the raw stream the same port carries, so that a
 * channel can be played from a MIDI port as well as read control by control.
 */
struct midi_device_protocol final
    : public ossia::net::protocol_base
    , public ossia::net::midi::midi_stream
{
  explicit midi_device_protocol(ProtocolSettings settings)
      : protocol_base{flags{}}
      , m_settings{std::move(settings)}
  {
    if(!m_settings.input && !m_settings.output)
      throw std::runtime_error("no MIDI port to talk to");
    if(m_settings.devices.empty() && m_settings.generic.empty())
      throw std::runtime_error("no device description to build a tree from");

    for(auto& d : m_settings.devices)
      d.channel = std::clamp(d.channel, 1, 16);

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

    for(const auto& d : m_settings.devices)
      for(const auto& w : d.map.warnings)
        qWarning() << "MIDI device map:" << d.map.label().c_str() << ":" << w.c_str();
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
  static int sendChannel(const binding& b) noexcept
  {
    const auto& m = b.control->message;
    return m.channels.empty() ? b.defaultChannel : m.channels.front();
  }

  void buildTree(ossia::net::node_base& root)
  {
    std::size_t total = 0;
    for(const auto& d : m_settings.devices)
      for(const auto& c : d.map.controls)
        total += isNoteName(c) ? 0 : 1;
    m_bindings.reserve(total);

    for(const auto& d : m_settings.devices)
    {
      // One level per description: two of the same model on one cable are two
      // devices, and merging their levels would point both at one channel.
      auto* under = root.create_child(deviceNodeName(d.map));
      if(!under)
        continue;

      m_levelChannel[under] = d.channel;

      for(const auto& c : d.map.controls)
      {
        // Note names are what the description says a note means, not something
        // to send: they belong to whatever comes to read them, not to the tree.
        if(isNoteName(c))
          continue;
        addControl(*under, c, d.channel);
      }
    }

    for(const auto& g : m_settings.generic)
      addGenericChannel(root, g);
  }

  /**
   * The same shape ossia's plain MIDI device builds, under a level named by
   * the channel: `1/on`, `1/control`, and with @ref GenericChannel::expanded
   * a node per number under each.
   */
  void addGenericChannel(ossia::net::node_base& root, const GenericChannel& g)
  {
    const int ch = std::clamp(g.channel, 1, 16);

    auto* under = root.create_child(std::to_string(ch));
    if(!under)
      return;

    m_levelChannel[under] = ch;

    m_rawChannels.push_back(std::make_unique<raw_channel>());
    auto& c = *m_rawChannels.back();
    c.channel = ch;

    const auto pair = [&](const char* name, raw_address::Kind kind) {
      auto* param = addRawNode(*under, name, ossia::val_type::LIST, kind, &c, -1);
      if(param)
        param->set_domain(
            ossia::make_domain(std::vector<ossia::value>{0, 0},
                               std::vector<ossia::value>{127, 127}));
      return param;
    };

    c.on = pair("on", raw_address::NoteOn);
    c.off = pair("off", raw_address::NoteOff);
    c.control = pair("control", raw_address::Control);

    c.program = addRawNode(
        *under, "program", ossia::val_type::INT, raw_address::Program, &c, -1);
    if(c.program)
      c.program->set_domain(ossia::make_domain(0, 127));

    c.pitchbend = addRawNode(
        *under, "pitchbend", ossia::val_type::INT, raw_address::PitchBend, &c, -1);
    if(c.pitchbend)
    {
      c.pitchbend->set_domain(ossia::make_domain(0, 16383));
      c.pitchbend->set_value(8192);
    }

    if(!g.expanded)
      return;

    /*
     * A node per number, under the node that carries it as a value: `on/60`
     * is the C the pair `on 60 <velocity>` also plays. A program change has
     * nothing left to say once its number is the address, so those are
     * impulses.
     */
    const auto numbered
        = [&](ossia::net::parameter_base* parent, raw_address::Kind kind,
              ossia::val_type type, std::vector<ossia::net::parameter_base*>& out) {
      if(!parent)
        return;
      auto& node = parent->get_node();
      out.resize(128);
      for(int i = 0; i < 128; i++)
      {
        out[i] = addRawNode(node, std::to_string(i), type, kind, &c, i);
        if(out[i] && type == ossia::val_type::INT)
          out[i]->set_domain(ossia::make_domain(0, 127));
      }
    };

    numbered(c.on, raw_address::NoteOn, ossia::val_type::INT, c.noteOn);
    numbered(c.off, raw_address::NoteOff, ossia::val_type::INT, c.noteOff);
    numbered(c.control, raw_address::Control, ossia::val_type::INT, c.controls);
    numbered(c.program, raw_address::Program, ossia::val_type::IMPULSE, c.programs);
  }

  ossia::net::parameter_base* addRawNode(
      ossia::net::node_base& parent, const std::string& name, ossia::val_type type,
      raw_address::Kind kind, raw_channel* chan, int number)
  {
    auto* node = parent.create_child(name);
    if(!node)
      return nullptr;

    auto* param = node->create_parameter(type);
    if(!param)
      return nullptr;

    param->set_access(accessMode(Direction::Both));
    m_rawOf[param] = raw_address{chan, kind, number};
    return param;
  }

  void addControl(ossia::net::node_base& root, const Control& c, int defaultChannel)
  {
    auto& parent = groupNode(root, c.group);
    auto* node = parent.create_child(c.name);
    if(!node)
      return;

    auto* param = node->create_parameter(ossia::val_type::INT);
    if(!param)
      return;

    auto [lo, hi] = resolvedRange(c.value, c.message.type);

    // A control that addresses a bank of notes holds which note of the bank,
    // so the bank is its range -- the note's own 0..127 is not reachable.
    if(c.message.hasRange() && !c.value.min && !c.value.max)
    {
      lo = c.message.rangeFrom;
      hi = c.message.rangeTo;
    }

    m_bindings.push_back(std::make_unique<binding>());
    auto& b = *m_bindings.back();
    b.param = param;
    b.control = &c;
    b.defaultChannel = defaultChannel;
    b.lo = lo;
    b.hi = hi;
    indexBinding(b);
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
    if(const auto raw = m_rawOf.find(&p); raw != m_rawOf.end())
      return sendRaw(raw->second, v);

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

  //! The two numbers of a node that carries one as half of its value.
  static std::pair<int, int> numberAndValue(const ossia::value& v) noexcept
  {
    const auto l = ossia::convert<std::vector<ossia::value>>(v);
    const auto at = [&l](std::size_t i) {
      return i < l.size() ? std::clamp(ossia::convert<int>(l[i]), 0, 127) : 0;
    };
    return {at(0), at(1)};
  }

  /**
   * A raw node sends what it is written and keeps nothing: one channel of MIDI
   * is whatever the device at the other end makes of it.
   *
   * Writing the pair also moves the numbered node it covers, and the other way
   * round, so the two ways of saying the same thing agree. set_value() rather
   * than push_value(): the twin has already been sent.
   */
  bool sendRaw(const raw_address& a, const ossia::value& v)
  {
    using ce = libremidi::channel_events;
    auto& c = *a.chan;
    const int ch = c.channel;

    const auto numbered
        = [](const std::vector<ossia::net::parameter_base*>& nodes, int i) {
      return i >= 0 && std::size_t(i) < nodes.size() ? nodes[i] : nullptr;
    };
    const auto pairOf = [](ossia::net::parameter_base* p, int number, int value) {
      if(p)
        p->set_value(std::vector<ossia::value>{number, value});
    };

    switch(a.kind)
    {
      case raw_address::NoteOn:
      case raw_address::NoteOff:
      {
        const bool off = a.kind == raw_address::NoteOff;
        const auto& nodes = off ? c.noteOff : c.noteOn;

        int note = a.number;
        int velocity = 0;
        if(a.number < 0)
          std::tie(note, velocity) = numberAndValue(v);
        else
          velocity = std::clamp(ossia::convert<int>(v), 0, 127);

        if(a.number < 0)
        {
          if(auto* twin = numbered(nodes, note))
            twin->set_value(velocity);
        }
        else
        {
          pairOf(off ? c.off : c.on, note, velocity);
        }

        // A note on at no velocity is a note off, which is how most hardware
        // releases a note and what the plain MIDI device does with one.
        if(off || velocity == 0)
          write(ce::note_off(ch, note, velocity));
        else
          write(ce::note_on(ch, note, velocity));
        return bool(m_output);
      }

      case raw_address::Control:
      {
        int number = a.number;
        int value = 0;
        if(a.number < 0)
        {
          std::tie(number, value) = numberAndValue(v);
          if(auto* twin = numbered(c.controls, number))
            twin->set_value(value);
        }
        else
        {
          value = std::clamp(ossia::convert<int>(v), 0, 127);
          pairOf(c.control, number, value);
        }

        write(ce::control_change(ch, number, value));
        return bool(m_output);
      }

      case raw_address::Program:
      {
        const int number = a.number >= 0
                               ? a.number
                               : std::clamp(ossia::convert<int>(v), 0, 127);
        if(a.number < 0)
        {
          if(auto* twin = numbered(c.programs, number))
            twin->set_value(ossia::impulse{});
        }
        else if(c.program)
        {
          c.program->set_value(number);
        }

        write(ce::program_change(ch, number));
        return bool(m_output);
      }

      case raw_address::PitchBend:
        write(ce::pitch_bend(ch, std::clamp(ossia::convert<int>(v), 0, 16383)));
        return bool(m_output);
    }
    return false;
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
    const int ch = sendChannel(b);

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
    // Before anything is read out of it: a MIDI port is given the message as
    // it came, whatever the description makes of it afterwards.
    if(m_streaming.load(std::memory_order_relaxed))
    {
      m_toUmp.convert(
          m.bytes.data(), m.bytes.size(), m.timestamp,
          [this](const uint32_t* ump, int count, auto ts) {
        libremidi::ump u;
        std::copy_n(ump, std::min(count, 4), u.data);
        u.timestamp = ts;
        receive_ump(u);
        return stdx::error{};
      });
    }

    if(m.size() < 2)
      return;

    const int status = m.get_message_type() == libremidi::message_type::INVALID
                           ? 0
                           : (m.bytes[0] & 0xF0);
    const int channel = (m.bytes[0] & 0x0F) + 1;
    const int d1 = m.bytes[1] & 0x7F;
    const int d2 = m.size() > 2 ? (m.bytes[2] & 0x7F) : 0;

    if(status == 0xB0)
      onParameterNumber(channel, d1, d2);

    receiveRaw(status, channel, d1, d2);

    // An instrument's description runs to thousands of controls, and one of
    // its messages addresses at most a handful of them.
    const auto addressed = m_byMessage.find(keyOf(status, channel, addressByte(status, d1)));
    if(addressed == m_byMessage.end())
      return;

    for(auto* held : addressed->second)
    {
      auto& b = *held;
      const auto& msg = b.control->message;

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
          // The coarse half is latched, not consumed: a slow move sends it
          // once and then a run of fine halves inside that step.
          else if(d1 == msg.lsb && b.pendingMsb >= 0)
            receive(b, (b.pendingMsb << 7) | d2);
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
          // Carried by four control changes; onParameterNumber follows them.
          break;
      }
    }
  }

  /**
   * Follow the control changes that carry an NRPN or RPN.
   *
   * The parameter is named by one pair of control changes and written by
   * another, so a value only exists once four messages have arrived, and the
   * selection stays latched between them: a device may write the same
   * parameter again with nothing but a further data entry.
   */
  /**
   * A raw channel follows the wire and nothing else: set_value(), so that what
   * arrives is not sent straight back out.
   */
  void receiveRaw(int status, int channel, int d1, int d2)
  {
    const auto numbered
        = [](const std::vector<ossia::net::parameter_base*>& nodes, int i, auto value) {
      if(i >= 0 && std::size_t(i) < nodes.size() && nodes[i])
        nodes[i]->set_value(value);
    };
    const auto pair = [](ossia::net::parameter_base* p, int a, int b) {
      if(p)
        p->set_value(std::vector<ossia::value>{a, b});
    };

    for(auto& held : m_rawChannels)
    {
      auto& c = *held;
      if(c.channel != channel)
        continue;

      switch(status)
      {
        case 0x90:
          if(d2 > 0)
          {
            pair(c.on, d1, d2);
            numbered(c.noteOn, d1, d2);
            break;
          }
          [[fallthrough]];
        case 0x80:
          pair(c.off, d1, d2);
          numbered(c.noteOff, d1, d2);
          break;

        case 0xB0:
          pair(c.control, d1, d2);
          numbered(c.controls, d1, d2);
          break;

        case 0xC0:
          if(c.program)
            c.program->set_value(d1);
          numbered(c.programs, d1, ossia::impulse{});
          break;

        case 0xE0:
          if(c.pitchbend)
            c.pitchbend->set_value((d2 << 7) | d1);
          break;

        default:
          break;
      }
    }
  }

  void onParameterNumber(int channel, int cc, int value)
  {
    auto& sel = m_selected[channel - 1];
    switch(cc)
    {
      // Selecting a parameter drops the data entry held for the previous one:
      // a fine-only write that follows belongs to whatever this one turns out
      // to be, not to the coarse half of the last.
      case cc_nrpn_msb: sel.number = value; sel.registered = false; sel.data = -1; return;
      case cc_nrpn_lsb: sel.lsb = value;    sel.registered = false; sel.data = -1; return;
      case cc_rpn_msb:  sel.number = value; sel.registered = true;  sel.data = -1; return;
      case cc_rpn_lsb:  sel.lsb = value;    sel.registered = true;  sel.data = -1; return;

      // Both halves dispatch a 14-bit value, so that a device that sends only
      // the coarse half still writes the value it means: dispatchParameter
      // narrows it to what the control is.
      case cc_data_msb:
        sel.data = value;
        dispatchParameter(channel, sel, value << 7);
        return;

      case cc_data_lsb:
        if(sel.data >= 0)
          dispatchParameter(channel, sel, (sel.data << 7) | value);
        return;

      default:
        return;
    }
  }

  void dispatchParameter(int channel, const selection& sel, int value)
  {
    if(sel.number < 0)
      return;

    const auto wanted = sel.registered ? MessageType::RPN : MessageType::NRPN;

    const auto named = m_byNumber.find(parameterKey(sel.registered, sel.number));
    if(named == m_byNumber.end())
      return;

    for(auto* held : named->second)
    {
      auto& b = *held;
      const auto& msg = b.control->message;
      if(msg.type != wanted || msg.number != sel.number)
        continue;
      if(msg.lsb >= 0 && msg.lsb != sel.lsb)
        continue;
      if(!acceptsChannel(b, channel))
        continue;

      receive(b, is14Bit(*b.control) ? value : (value >> 7));
    }
  }

  /**
   * What a message addresses, as one number: its kind, its channel, and the
   * data byte that is an address rather than a value.
   *
   * The dispatch is a lookup on this rather than a walk, because a MIDNAM
   * instrument states thousands of parameters and a controller can stream a
   * thousand messages a second into the callback thread the audio graph reads.
   */
  static constexpr uint32_t keyOf(int status, int channel, int number) noexcept
  {
    return (uint32_t(status & 0xF0) << 16) | (uint32_t(channel & 0x1F) << 8)
           | uint32_t(number & 0xFF);
  }

  //! Program change, pitch bend and channel aftertouch carry a value where the
  //! others carry an address, so every one of them reaches the same bucket.
  static constexpr int addressByte(int status, int d1) noexcept
  {
    switch(status)
    {
      case 0x80:
      case 0x90:
      case 0xA0:
      case 0xB0:
        return d1;
      default:
        return 0;
    }
  }

  //! @see keyOf, for the parameter numbers, which no status byte names.
  static constexpr uint32_t parameterKey(bool registered, int number) noexcept
  {
    return (uint32_t(registered) << 8) | uint32_t(number & 0xFF);
  }

  void indexBinding(binding& b)
  {
    const auto& m = b.control->message;
    const std::vector<int> channels
        = m.channels.empty() ? std::vector<int>{b.defaultChannel} : m.channels;

    const auto add = [&](int status, int number) {
      for(const int ch : channels)
        m_byMessage[keyOf(status, ch, number)].push_back(&b);
    };

    switch(m.type)
    {
      case MessageType::CC:
        add(0xB0, m.number);
        break;
      case MessageType::CC14:
        add(0xB0, m.number);
        add(0xB0, m.lsb);
        break;
      case MessageType::Note:
        if(m.hasRange())
          for(int n = m.rangeFrom; n <= m.rangeTo; n++)
          {
            add(0x90, n);
            add(0x80, n);
          }
        else
        {
          add(0x90, m.number);
          add(0x80, m.number);
        }
        break;
      case MessageType::Program:
        add(0xC0, 0);
        break;
      case MessageType::PitchBend:
        add(0xE0, 0);
        break;
      case MessageType::Aftertouch:
        add(0xD0, 0);
        break;
      case MessageType::PolyAftertouch:
        add(0xA0, m.number);
        break;
      case MessageType::NRPN:
      case MessageType::RPN:
        // Four control changes name these; onParameterNumber follows them.
        m_byNumber[parameterKey(m.type == MessageType::RPN, m.number)].push_back(&b);
        break;
    }
  }

  /**
   * A description that states no channel means "whichever the device is set
   * to", and the slot it was loaded under is where the user said that. Taking
   * it as "any channel" would make every device on a chain answer for all the
   * others.
   */
  static bool acceptsChannel(const binding& b, int channel) noexcept
  {
    const auto& m = b.control->message;
    if(m.channels.empty())
      return channel == b.defaultChannel;
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

  libremidi::midi_in* midi_in() const noexcept override { return m_input.get(); }

  void enable_registration() override { m_streaming.store(true); }

  void push_value(const libremidi::message& m) override { write(m); }

  void push_value(const libremidi::ump& m) override
  {
    if(m_output)
      m_output->send_ump(m);
  }

  /**
   * A level of the tree stands for one device: a raw channel for the channel
   * it was added on, a description for the channel its controls default to.
   * The root stands for the port, and hears all sixteen.
   */
  std::optional<int> stream_channel(const ossia::net::node_base& n) const noexcept override
  {
    if(const auto it = m_levelChannel.find(&n); it != m_levelChannel.end())
      return it->second;
    return std::nullopt;
  }

  bool push_raw(const ossia::net::full_parameter_data&) override { return false; }
  bool observe(ossia::net::parameter_base&, bool) override { return true; }
  bool update(ossia::net::node_base&) override { return false; }

  ProtocolSettings m_settings;

  std::optional<libremidi::input_port> m_resolvedInput;
  std::unique_ptr<libremidi::midi_in> m_input;
  std::unique_ptr<libremidi::midi_out> m_output;

  //! One per MIDI channel: a device may have a different parameter selected on
  //! each, and the selection outlives the message that set it.
  std::array<selection, 16> m_selected{};

  //! Stable addresses: m_bindingOf points into them.
  std::vector<std::unique_ptr<binding>> m_bindings;

  //! What each message addresses, so that dispatching one is a lookup rather
  //! than a walk. @see keyOf, parameterKey
  ossia::hash_map<uint32_t, std::vector<binding*>> m_byMessage;
  ossia::hash_map<uint32_t, std::vector<binding*>> m_byNumber;

  //! A control's numeric node and its choice both resolve to the one binding;
  //! push() tells them apart by identity.
  ossia::hash_map<const ossia::net::parameter_base*, binding*> m_bindingOf;

  //! Stable addresses: m_rawOf points into them.
  std::vector<std::unique_ptr<raw_channel>> m_rawChannels;

  ossia::hash_map<const ossia::net::parameter_base*, raw_address> m_rawOf;

  //! The channel each level of the tree stands for. @see stream_channel
  ossia::hash_map<const ossia::net::node_base*, int> m_levelChannel;

  //! Whether anything reads this port as a stream: the conversion to MIDI 2
  //! and the queue are not worth paying for otherwise.
  std::atomic_bool m_streaming{};

  libremidi::midi1_to_midi2 m_toUmp;
};
}

std::unique_ptr<ossia::net::protocol_base> makeProtocol(ProtocolSettings settings)
{
  return std::make_unique<midi_device_protocol>(std::move(settings));
}

}
