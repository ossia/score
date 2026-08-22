#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_CAN)
#include "CANDevice.hpp"

#include "CANSpecificSettings.hpp"
#include "DBCParser.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/detail/hash_map.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/sockets/can_socket.hpp>

#include <QDebug>

#include <wobjectimpl.h>

#include <cmath>

W_OBJECT_IMPL(Protocols::CANDevice)

namespace ossia::net
{
namespace
{
using namespace Protocols::CAN;

/**
 * Key for the frame dispatch table.
 *
 * The identifier alone is *not* a key: an 11-bit standard frame and a 29-bit
 * extended frame may carry the same numeric identifier and are different
 * frames on the bus. The extended flag therefore goes into the key.
 */
constexpr uint64_t frameKey(uint32_t id, bool extended) noexcept
{
  return (uint64_t(extended) << 32) | uint64_t(id);
}

/**
 * Map a DBC unit string onto an ossia unit.
 *
 * Tiny on purpose: DBC units are free text with no vocabulary, and most of what
 * vendor files use ("g", "d/s", "uT", "kPa") has no ossia dataspace. A wrong
 * unit brings a wrong automatic conversion with it, which is worse than none -
 * an unmapped signal is just a number, which is what it is. Only unambiguous
 * units that exist in ossia are mapped.
 */
ossia::unit_t unitFromDBC(std::string_view u)
{
  // Angles: the only dataspace the vendor files actually touch.
  if(u == "d" || u == "deg" || u == "degree" || u == "degrees" || u == "°")
    return ossia::degree_u{};
  if(u == "rad" || u == "radian" || u == "radians")
    return ossia::radian_u{};

  // Unambiguous symbols only: "m" is left out, being as often "minutes" or a
  // scale prefix in an automotive database.
  if(u == "km")
    return ossia::kilometer_u{};
  if(u == "cm")
    return ossia::centimeter_u{};
  if(u == "mm")
    return ossia::millimeter_u{};

  return {};
}

/**
 * Choose the ossia value type for a signal.
 *
 * A signal whose scaling is the identity is an integer, and presenting it as a
 * float would lose exactness on the wide ones; anything scaled is a float. A
 * single bit with identity scaling is a boolean.
 */
ossia::val_type valueTypeFor(const Signal& sig) noexcept
{
  if(sig.valueType == ValueType::Float32 || sig.valueType == ValueType::Double64)
    return ossia::val_type::FLOAT;

  const bool identity = (sig.factor == 1. && sig.offset == 0.);
  if(!identity)
    return ossia::val_type::FLOAT;

  if(!sig.valueTable.empty())
    return ossia::val_type::INT;

  if(sig.length == 1)
    return ossia::val_type::BOOL;

  // ossia's INT is a 32-bit signed integer: only widths that fit in it without
  // wrapping may use it. An unsigned 32-bit signal does not.
  const bool isSigned = (sig.valueType == ValueType::Signed);
  if(isSigned ? (sig.length <= 32) : (sig.length <= 31))
    return ossia::val_type::INT;

  return ossia::val_type::FLOAT;
}

//! One signal, resolved to the parameter it feeds.
struct signal_binding
{
  const Signal* sig{};
  ossia::net::parameter_base* param{};
  ossia::val_type type{};
};

//! One message, resolved to its signals' parameters.
struct message_binding
{
  const Message* msg{};
  std::vector<signal_binding> signals;

  //! The `M` signal of the message, if it has one.
  const Signal* multiplexer{};
};

/**
 * A read-only CAN device driven by a DBC database.
 *
 * Incoming frames are dispatched through a hash map built once, when the node
 * tree is created -- never by looking up a node by name per frame. A CAN bus
 * at 1 Mbit/s delivers on the order of 8000 frames per second and every one of
 * them lands on this path.
 */
struct can_protocol final : public ossia::net::protocol_base
{
  using settings = Protocols::CANSpecificSettings;

  can_protocol(const settings& set, ossia::net::network_context_ptr ctx)
      : protocol_base{flags{}}
      , m_context{std::move(ctx)}
      , m_db{loadDatabase(set)}
      , m_socket{makeConfiguration(set, m_db), m_context->context}
  {
    // Throws std::system_error if the interface does not exist or cannot be
    // bound; the device's reconnect() turns that into a message for the user.
    m_socket.open();
  }

  ~can_protocol() { m_socket.close(); }

  /**
   * Read the database and apply the two settings that rewrite it.
   *
   * Order matters: the float override is about how a signal's *bits* are read
   * and the offset is about *which frame* carries it, so they are independent,
   * but both must happen before the dispatch table keys are computed.
   */
  static Database loadDatabase(const settings& set)
  {
    if(set.dbcPath.isEmpty())
      throw std::runtime_error("no DBC file given");

    auto db = parseDBCFile(set.dbcPath.toStdString());

    // Diagnostics are never silent: a half-loaded database is worse than one
    // that fails, so everything the parser could not make sense of is said out
    // loud even though it is not fatal.
    for(const auto& w : db.warnings)
      qWarning() << "CAN: " << set.dbcPath << ":" << w.c_str();

    if(db.messages.empty())
      throw std::runtime_error(
          "no message could be read from " + set.dbcPath.toStdString());

    if(set.float32Override)
      applyFloat32Override(db);

    applyNodeIdOffset(db, set.nodeIdOffset);

    // applyNodeIdOffset may itself complain about messages it could not move.
    for(std::size_t k = db.warnings.size(); k-- > 0;)
      if(db.warnings[k].rfind("node id offset", 0) == 0)
        qWarning() << "CAN: " << db.warnings[k].c_str();

    return db;
  }

  /**
   * Build the socket configuration, including a kernel-side filter limited to
   * the identifiers the database actually describes.
   *
   * Worth doing: on a shared bus this is what keeps a chain of devices cheap.
   * SocketCAN applies filters per socket, so each score device wakes only for
   * its own node's frames instead of for every frame on the wire.
   */
  static can_configuration makeConfiguration(const settings& set, const Database& db)
  {
    can_configuration conf;
    conf.interface_name = set.interfaceName.toStdString();
    conf.fd = set.fd;

    if(set.filterToDatabase)
    {
      for(const auto& msg : db.messages)
      {
        can_filter_configuration f;
        // The filter is matched against the raw identifier, flag bits and all,
        // so the extended flag has to be put back and the mask has to cover it
        // -- otherwise a standard frame would match an extended filter.
        if(msg.extended)
        {
          f.id = (msg.id & CAN_EFF_MASK) | CAN_EFF_FLAG;
          f.mask = CAN_EFF_MASK | CAN_EFF_FLAG;
        }
        else
        {
          f.id = msg.id & CAN_SFF_MASK;
          f.mask = CAN_SFF_MASK | CAN_EFF_FLAG;
        }
        conf.filters.push_back(f);
      }
    }

    return conf;
  }

  void set_device(ossia::net::device_base& dev) override
  {
    m_device = &dev;
    buildTree(dev.get_root_node());

    m_socket.receive([this](const can_message& msg) { onFrame(msg); });
  }

  void buildTree(ossia::net::node_base& root)
  {
    m_bindings.reserve(m_db.messages.size());

    for(const auto& msg : m_db.messages)
    {
      // create_node rather than find_or_create_node: two messages may share a
      // name (they are keyed by identifier, not by name), and merging them
      // into one node would silently mix two frames' signals.
      auto& msgNode = ossia::net::create_node(root, msg.name);
      if(!msg.comment.empty())
        ossia::net::set_description(msgNode, msg.comment);

      message_binding binding;
      binding.msg = &msg;
      binding.signals.reserve(msg.signals.size());

      for(const auto& sig : msg.signals)
      {
        const auto type = valueTypeFor(sig);

        auto& sigNode = ossia::net::create_node(msgNode, sig.name);
        auto* param = sigNode.create_parameter(type);
        if(!param)
          continue;

        applyMetadata(sigNode, *param, sig, type);

        binding.signals.push_back(signal_binding{&sig, param, type});

        if(sig.isMultiplexer)
          binding.multiplexer = &sig;
      }

      m_bindings.emplace(frameKey(msg.id, msg.extended), std::move(binding));
    }
  }

  static void applyMetadata(
      ossia::net::node_base& node, ossia::net::parameter_base& param, const Signal& sig,
      ossia::val_type type)
  {
    // Receive-only: the value comes from the bus.
    param.set_access(ossia::access_mode::GET);

    const auto unit = unitFromDBC(sig.unit);
    if(unit)
      param.set_unit(unit);

    if(!sig.valueTable.empty())
    {
      // A value table is an enumeration: the domain is the set of the values
      // the signal is documented to take.
      std::vector<ossia::value> accepted;
      accepted.reserve(sig.valueTable.size());
      for(const auto& vd : sig.valueTable)
        accepted.push_back(int32_t(vd.value));
      param.set_domain(ossia::make_domain(accepted));
    }
    else if(sig.hasRange && type != ossia::val_type::BOOL)
    {
      // `[0|0]` means "no range stated" and the parser reports it as such;
      // pushing it as a domain would clamp every value to zero.
      //
      // The bounds must carry the parameter's own type: a float domain on an
      // integer parameter is a type mismatch that ossia has to reconcile on
      // every clamp.
      if(type == ossia::val_type::INT)
        param.set_domain(ossia::make_domain(int32_t(sig.min), int32_t(sig.max)));
      else
        param.set_domain(ossia::make_domain(float(sig.min), float(sig.max)));
    }

    // The description carries what has no structured place in ossia: the DBC
    // comment, the unit when it could not be mapped, and the enumeration's
    // names.
    std::string desc = sig.comment;
    if(!sig.unit.empty() && !unit)
    {
      if(!desc.empty())
        desc += ' ';
      desc += '[' + sig.unit + ']';
    }
    if(!sig.valueTable.empty())
    {
      if(!desc.empty())
        desc += ' ';
      desc += '{';
      bool first = true;
      for(const auto& vd : sig.valueTable)
      {
        if(!first)
          desc += ", ";
        first = false;
        desc += std::to_string(vd.value) + '=' + vd.name;
      }
      desc += '}';
    }
    if(!desc.empty())
      ossia::net::set_description(node, desc);
  }

  /**
   * Decode one frame.
   *
   * The hot path: one hash lookup, then one pass over the message's signals.
   */
  void onFrame(const can_message& msg)
  {
    // Error frames are not bus traffic -- the identifier is a bitmask of error
    // classes, not an identifier -- and an RTR frame carries no payload at all,
    // so decoding either would produce numbers out of nothing.
    if(msg.error || msg.remote)
      return;

    const auto it = m_bindings.find(frameKey(msg.id, msg.extended));
    if(it == m_bindings.end())
      return;

    const auto& binding = it->second;
    const int size = int(msg.size);

    // Simple multiplexing: the switch signal selects which of the multiplexed
    // signals this particular frame actually carries.
    int64_t muxValue = 0;
    const bool multiplexed = (binding.multiplexer != nullptr);
    if(multiplexed)
      muxValue = int64_t(rawSignalBits(*binding.multiplexer, msg.data, size));

    for(const auto& sb : binding.signals)
    {
      if(multiplexed && sb.sig->isMultiplexed && sb.sig->multiplexValue != muxValue)
        continue;

      const double v = decodeSignal(*sb.sig, msg.data, size);

      switch(sb.type)
      {
        case ossia::val_type::BOOL:
          sb.param->push_value(v != 0.);
          break;
        case ossia::val_type::INT:
          sb.param->push_value(int32_t(std::llround(v)));
          break;
        default:
          sb.param->push_value(float(v));
          break;
      }
    }
  }

  // Receive-only for now: see the note in CANProtocolFactory on transmit.
  bool pull(parameter_base&) override { return false; }
  bool push(const parameter_base&, const value&) override { return false; }
  bool push_raw(const full_parameter_data&) override { return false; }
  bool observe(parameter_base&, bool) override { return false; }
  bool update(node_base&) override { return false; }

  ossia::net::network_context_ptr m_context;
  ossia::net::device_base* m_device{};

  //! Owns the Message and Signal objects that the bindings point into. It is
  //! not touched after the constructor, so those pointers stay valid.
  Database m_db;

  ossia::hash_map<uint64_t, message_binding> m_bindings;

  //! Declared last on purpose. Members are destroyed in reverse declaration
  //! order, so the socket -- and with it the liveness token that the receive
  //! handler holds -- goes away before the database and the dispatch table
  //! that handler reads from. It is also constructed last, which is required
  //! anyway: its configuration is derived from m_db.
  can_socket m_socket;
};
}
}

namespace Protocols
{

CANDevice::CANDevice(
    const Device::DeviceSettings& settings, const ossia::net::network_context_ptr& ctx)
    : OwningDeviceInterface{settings}
    , m_ctx{ctx}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canSerialize = false;
}

CANDevice::~CANDevice() { }

bool CANDevice::reconnect()
{
  disconnect();

  try
  {
    const auto& set = m_settings.deviceSpecificSettings.value<CANSpecificSettings>();

    // Named explicitly rather than left to SocketCAN: an empty name reaches the
    // kernel as "no such CAN interface: : No such device", which reads like a
    // bug in score rather than like a field the user has to fill in.
    if(set.interfaceName.isEmpty())
      throw std::runtime_error(
          "no CAN interface selected -- pick one in the device settings");

    auto proto = std::make_unique<ossia::net::can_protocol>(set, m_ctx);
    auto dev = std::make_unique<ossia::net::generic_device>(
        std::move(proto), settings().name.toStdString());
    m_dev = std::move(dev);

    deviceChanged(nullptr, m_dev.get());
  }
  catch(const std::exception& e)
  {
    // qWarning, not qDebug: a device that failed to connect shows up in the
    // explorer as an empty tree with no other explanation, and the reason must
    // survive a build where debug output is filtered out.
    qWarning() << "CAN device" << settings().name << "could not connect:" << e.what();
  }
  catch(...)
  {
    qWarning() << "CAN device" << settings().name
               << "could not connect: unknown error";
  }

  return connected();
}

void CANDevice::disconnect()
{
  OwningDeviceInterface::disconnect();
}
}
#endif
