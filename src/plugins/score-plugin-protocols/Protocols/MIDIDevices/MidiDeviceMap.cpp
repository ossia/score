#include "MidiDeviceMap.hpp"

#include <rapidjson/document.h>
#include <rapidjson/reader.h>

#include <algorithm>
#include <array>
#include <charconv>

namespace Protocols::MIDIDevices
{

namespace
{
using json_value = rapidjson::Value;

constexpr std::string_view format_id = "score.midi-device/1";

template <typename T, std::size_t N>
std::optional<T>
lookup(const std::array<std::pair<std::string_view, T>, N>& table, std::string_view s)
{
  for(const auto& [name, v] : table)
    if(name == s)
      return v;
  return std::nullopt;
}

constexpr std::array<std::pair<std::string_view, MessageType>, 9> message_types{
    {{"cc", MessageType::CC},
     {"cc14", MessageType::CC14},
     {"nrpn", MessageType::NRPN},
     {"rpn", MessageType::RPN},
     {"note", MessageType::Note},
     {"pitchbend", MessageType::PitchBend},
     {"aftertouch", MessageType::Aftertouch},
     {"poly_aftertouch", MessageType::PolyAftertouch},
     {"program", MessageType::Program}}};

constexpr std::array<std::pair<std::string_view, Kind>, 11> kinds{
    {{"fader", Kind::Fader},
     {"knob", Kind::Knob},
     {"encoder", Kind::Encoder},
     {"button", Kind::Button},
     {"pad", Kind::Pad},
     {"key", Kind::Key},
     {"wheel", Kind::Wheel},
     {"ribbon", Kind::Ribbon},
     {"xy", Kind::XY},
     {"parameter", Kind::Parameter},
     {"other", Kind::Other}}};

constexpr std::array<std::pair<std::string_view, Direction>, 3> directions{
    {{"in", Direction::In}, {"out", Direction::Out}, {"both", Direction::Both}}};

constexpr std::array<std::pair<std::string_view, Encoding>, 4> encodings{
    {{"twos_complement", Encoding::TwosComplement},
     {"signed_bit", Encoding::SignedBit},
     {"signed_bit_2", Encoding::SignedBit2},
     {"binary_offset", Encoding::BinaryOffset}}};

const json_value* member(const json_value& obj, const char* name)
{
  if(!obj.IsObject())
    return nullptr;
  auto it = obj.FindMember(name);
  return it != obj.MemberEnd() ? &it->value : nullptr;
}

std::string str(const json_value& obj, const char* name)
{
  const auto* v = member(obj, name);
  return v && v->IsString() ? std::string(v->GetString(), v->GetStringLength())
                            : std::string{};
}

std::optional<int> num(const json_value& obj, const char* name)
{
  const auto* v = member(obj, name);
  if(v && v->IsInt())
    return v->GetInt();
  return std::nullopt;
}

bool flag(const json_value& obj, const char* name)
{
  const auto* v = member(obj, name);
  return v && v->IsBool() && v->GetBool();
}

std::string_view trim(std::string_view s)
{
  const auto ws = " \t\r\n";
  const auto b = s.find_first_not_of(ws);
  if(b == std::string_view::npos)
    return {};
  return s.substr(b, s.find_last_not_of(ws) - b + 1);
}

/**
 * The two ways a document may write a group. An array states the levels
 * outright and each member is taken literally, which is the only way to write a
 * level whose own name contains a colon; a string nests on `:`, with whitespace
 * around the separator insignificant.
 */
std::vector<std::string> parseGroup(const json_value& v)
{
  std::vector<std::string> out;
  if(v.IsArray())
  {
    for(const auto& lvl : v.GetArray())
      if(lvl.IsString())
        if(auto t = trim({lvl.GetString(), lvl.GetStringLength()}); !t.empty())
          out.emplace_back(t);
  }
  else if(v.IsString())
  {
    std::string_view s{v.GetString(), v.GetStringLength()};
    for(std::size_t pos = 0; pos <= s.size();)
    {
      const auto sep = s.find(':', pos);
      const auto piece
          = trim(s.substr(pos, sep == std::string_view::npos ? sep : sep - pos));
      if(!piece.empty())
        out.emplace_back(piece);
      if(sep == std::string_view::npos)
        break;
      pos = sep + 1;
    }
  }
  return out;
}

/**
 * @p stated is false only for a document that says nothing about the channel,
 * which means "whichever the device is set to". A document that names channels
 * and names none that exist is a different thing entirely, and must not be read
 * as the wildcard.
 */
std::vector<int> parseChannels(const json_value& v, bool& stated)
{
  stated = !v.IsNull();

  std::vector<int> out;
  if(v.IsInt())
    out.push_back(v.GetInt());
  else if(v.IsArray())
    for(const auto& c : v.GetArray())
      if(c.IsInt())
        out.push_back(c.GetInt());

  std::erase_if(out, [](int c) { return c < 1 || c > 16; });
  return out;
}

Bank parseBank(const json_value& v)
{
  Bank b;
  if(auto msb = num(v, "msb"))
    b.msb = *msb;
  if(auto lsb = num(v, "lsb"))
    b.lsb = *lsb;
  return b;
}

std::optional<Message> parseMessage(const json_value& v)
{
  if(!v.IsObject())
    return std::nullopt;

  const auto type = lookup(message_types, str(v, "type"));
  if(!type)
    return std::nullopt;

  Message m;
  m.type = *type;

  if(const auto* ch = member(v, "channel"))
  {
    bool stated = false;
    m.channels = parseChannels(*ch, stated);
    if(stated && m.channels.empty())
      return std::nullopt;
  }
  if(auto n = num(v, "number"))
    m.number = *n;
  if(auto l = num(v, "lsb"))
    m.lsb = *l;
  if(const auto* b = member(v, "bank"))
    m.bank = parseBank(*b);
  if(const auto* r = member(v, "range"))
  {
    if(auto from = num(*r, "from"), to = num(*r, "to"); from && to)
    {
      m.rangeFrom = *from;
      m.rangeTo = *to;
    }
  }
  return m;
}

Scale parseScale(const json_value& v)
{
  Scale sc;
  if(!v.IsObject())
    return sc;

  for(const auto* name : {"min", "max"})
  {
    if(const auto* m = member(v, name); m && m->IsNumber())
      (std::string_view{name} == "min" ? sc.min : sc.max) = m->GetDouble();
  }
  sc.from = str(v, "from");
  sc.to = str(v, "to");
  sc.unit = str(v, "unit");
  return sc;
}

Value parseValue(const json_value& v)
{
  Value out;
  if(!v.IsObject())
    return out;

  if(str(v, "mode") == "relative")
    out.mode = ValueMode::Relative;
  if(auto e = lookup(encodings, str(v, "encoding")))
    out.encoding = *e;

  out.min = num(v, "min");
  out.max = num(v, "max");
  out.def = num(v, "default");
  out.bipolar = str(v, "orientation") == "bipolar";
  if(const auto* sc = member(v, "scale"))
    out.scale = parseScale(*sc);
  out.inverted = flag(v, "inverted");
  out.pickup = flag(v, "pickup");

  if(const auto* mom = member(v, "momentary"); mom && mom->IsBool())
    out.momentary = mom->GetBool();

  if(const auto* sends = member(v, "sends"); sends && sends->IsArray())
    for(const auto& s : sends->GetArray())
      if(s.IsInt())
        out.sends.push_back(s.GetInt());

  if(const auto* labels = member(v, "labels"); labels && labels->IsArray())
  {
    for(const auto& l : labels->GetArray())
    {
      auto from = num(l, "from"), to = num(l, "to");
      auto name = str(l, "name");
      if(from && to && !name.empty())
        out.labels.push_back({*from, *to, std::move(name)});
    }
  }
  return out;
}

void parseWhen(const json_value& v, std::vector<Condition>& out)
{
  const auto one = [&](const json_value& c) {
    if(auto p = num(c, "program"))
    {
      Condition cond;
      cond.program = *p;
      if(const auto* b = member(c, "bank"))
        cond.bank = parseBank(*b);
      out.push_back(cond);
    }
  };

  if(v.IsArray())
    for(const auto& c : v.GetArray())
      one(c);
  else
    one(v);
}

Preset parsePreset(const json_value& v)
{
  Preset p;
  if(v.IsString())
  {
    p.name.assign(v.GetString(), v.GetStringLength());
    return p;
  }
  if(!v.IsObject())
    return p;

  p.name = str(v, "name");
  if(const auto* sx = member(v, "sysex"))
  {
    if(sx->IsString())
      p.sysex.emplace_back(sx->GetString(), sx->GetStringLength());
    else if(sx->IsArray())
      for(const auto& m : sx->GetArray())
        if(m.IsString())
          p.sysex.emplace_back(m.GetString(), m.GetStringLength());
  }
  return p;
}

Source parseSource(const json_value& v)
{
  Source s;
  if(!v.IsObject())
    return s;

  s.format = str(v, "format");
  if(const auto* lic = member(v, "license"))
  {
    s.license = str(*lic, "spdx");
    s.redistribution = str(*lic, "redistribution");
    if(const auto* n = member(*lic, "notices"); n && n->IsArray())
      for(const auto& notice : n->GetArray())
        if(notice.IsString())
          s.notices.emplace_back(notice.GetString(), notice.GetStringLength());
  }

  // A declared author may carry only `verbatim`: a string naming three parties
  // has no one `name`, and picking one would drop the others.
  if(const auto* authors = member(v, "authors"); authors && authors->IsArray())
  {
    for(const auto& a : authors->GetArray())
    {
      auto who = str(a, "name");
      if(who.empty())
        who = str(a, "verbatim");
      if(!who.empty())
        s.authors.push_back(std::move(who));
    }
  }
  return s;
}

constexpr bool isDataByte(int v) noexcept
{
  return v >= 0 && v <= 127;
}

/**
 * Whether the message names something that can go on a MIDI cable.
 *
 * Every number here becomes a byte, and a byte with the top bit set is read as
 * the start of another message: an out-of-range address does not address the
 * wrong control, it corrupts the stream for everything downstream. A control
 * whose address cannot be sent is no more usable than one with no address.
 */
bool addressable(const Message& m, std::string& why)
{
  const auto needs = [&](bool ok, const char* what) {
    if(!ok && why.empty())
      why = what;
    return ok;
  };

  switch(m.type)
  {
    case MessageType::CC:
    case MessageType::NRPN:
    case MessageType::RPN:
      needs(isDataByte(m.number), "number is not a data byte");
      // The NRPN and RPN parameter number is a pair; its low half is optional
      // only in the sense that a document may leave it at zero.
      if(m.type != MessageType::CC)
        needs(m.lsb < 0 || isDataByte(m.lsb), "lsb is not a data byte");
      break;

    case MessageType::CC14:
      needs(isDataByte(m.number), "number is not a data byte");
      // Without both halves everything below 128 would go out as zero.
      needs(isDataByte(m.lsb), "cc14 needs an lsb");
      break;

    case MessageType::Note:
      if(m.rangeFrom >= 0 || m.rangeTo >= 0)
        needs(
            isDataByte(m.rangeFrom) && isDataByte(m.rangeTo) && m.rangeFrom <= m.rangeTo,
            "note range is not a range of notes");
      else
        needs(isDataByte(m.number), "number is not a note");
      break;

    case MessageType::PolyAftertouch:
      needs(isDataByte(m.number), "number is not a note");
      break;

    case MessageType::Program:
      needs(m.bank.msb < 0 || isDataByte(m.bank.msb), "bank msb is not a data byte");
      needs(m.bank.lsb < 0 || isDataByte(m.bank.lsb), "bank lsb is not a data byte");
      break;

    case MessageType::PitchBend:
    case MessageType::Aftertouch:
      break;
  }
  return why.empty();
}

std::optional<Control> parseControl(const json_value& v, std::string& why)
{
  if(!v.IsObject())
  {
    why = "not an object";
    return std::nullopt;
  }

  Control c;
  c.name = str(v, "name");
  if(c.name.empty())
  {
    why = "no name";
    return std::nullopt;
  }

  const auto* msg = member(v, "message");
  if(!msg)
  {
    why = "no message";
    return std::nullopt;
  }
  auto parsed = parseMessage(*msg);
  if(!parsed)
  {
    why = "unknown message type";
    return std::nullopt;
  }
  c.message = *parsed;

  if(auto k = lookup(kinds, str(v, "kind")))
    c.kind = *k;
  if(auto d = lookup(directions, str(v, "direction")))
    c.direction = *d;

  if(const auto* g = member(v, "group"))
    c.group = parseGroup(*g);
  if(const auto* fb = member(v, "feedback"))
    c.feedback = parseMessage(*fb);
  if(const auto* val = member(v, "value"))
    c.value = parseValue(*val);
  if(const auto* w = member(v, "when"))
    parseWhen(*w, c.when);
  if(const auto* l = member(v, "layer"))
  {
    c.layer.name = str(*l, "name");
    c.layer.of = str(*l, "of");
  }

  c.description = str(v, "description");

  // An endless encoder read as absolute produces 1 and 127 forever instead of a
  // delta, so a relative control with no encoding is worse than no control.
  if(c.value.mode == ValueMode::Relative && !c.value.encoding)
  {
    why = "relative without an encoding";
    return std::nullopt;
  }

  if(!addressable(c.message, why))
    return std::nullopt;

  // Anything but the two spellings is a control whose direction of travel is
  // unknown, and "absolute" is the reading that breaks an encoder silently.
  if(const auto* val = member(v, "value"))
  {
    if(const auto m = str(*val, "mode"); !m.empty() && m != "absolute" && m != "relative")
    {
      why = "unknown value mode '" + m + "'";
      return std::nullopt;
    }
  }

  return c;
}
}

namespace
{
/**
 * A SAX handler that reads the document's own fields without its controls.
 *
 * By default it stops at `controls`, which is what makes listing a library
 * cheap; rapidjson has no other way to say "stop", so the handler returns
 * false and @ref done rather than the reader's status says whether the header
 * was read.
 *
 * @ref skipControls walks past them instead, for a document that states them
 * before the fields this reads. Key order is not part of the format, so a
 * document must not become unreadable by choosing an unusual one.
 */
struct header_reader : rapidjson::BaseReaderHandler<rapidjson::UTF8<>, header_reader>
{
  explicit header_reader(bool skip) noexcept : skipControls{skip} { }

  DeviceHeader out;
  std::string format;
  bool done{};
  bool skipControls{};

  //! 0 outside the document, 1 inside it, deeper inside `preset` or an array.
  int depth{};
  std::string key;
  //! Which depth-1 member we are inside, empty between members.
  std::string section;

  //! The depth `controls` opened at, or 0 when not inside it.
  int skippingFrom{};

  //! The depth-1 members already seen: rapidjson's own reader keeps the first
  //! of a repeated key, and the two readers must not disagree about a
  //! document.
  std::vector<std::string> seen;

  bool skipping() const noexcept { return skippingFrom != 0; }

  bool Key(const char* str, rapidjson::SizeType len, bool)
  {
    if(skipping())
      return true;

    if(depth == 1)
    {
      key.assign(str, len);
      if(key == "controls")
      {
        if(!skipControls)
        {
          done = true;
          return false;
        }
        skippingFrom = depth;
        return true;
      }

      if(std::find(seen.begin(), seen.end(), key) != seen.end())
        key.clear();
      else
        seen.push_back(key);
      section = key;
    }
    else if(depth == 2)
      key.assign(str, len);
    return true;
  }

  bool String(const char* str, rapidjson::SizeType len, bool)
  {
    if(skipping())
      return true;

    std::string v(str, len);
    if(depth == 1)
    {
      if(key == "format")
        format = std::move(v);
      else if(key == "manufacturer")
        out.manufacturer = std::move(v);
      else if(key == "model")
        out.model = std::move(v);
      else if(key == "description")
        out.description = std::move(v);
      else if(key == "requires")
        out.requirement = std::move(v);
      else if(key == "preset")
        out.preset.name = std::move(v);
    }
    else if(section == "match")
      out.match.push_back(std::move(v));
    else if(section == "preset")
    {
      if(key == "name")
        out.preset.name = std::move(v);
      else if(key == "sysex")
        out.preset.sysex.push_back(std::move(v));
    }
    return true;
  }

  bool StartObject() { ++depth; return true; }
  bool EndObject(rapidjson::SizeType)
  {
    leave();
    if(depth == 0)
      done = true;
    return true;
  }
  bool StartArray() { ++depth; return true; }
  bool EndArray(rapidjson::SizeType)
  {
    leave();
    return true;
  }

  void leave() noexcept
  {
    --depth;
    if(skipping() && depth <= skippingFrom)
      skippingFrom = 0;
  }
};
}

std::string DeviceHeader::label() const
{
  if(manufacturer.empty())
    return model;
  if(model.empty())
    return manufacturer;
  return manufacturer + ": " + model;
}

namespace
{
std::optional<DeviceHeader> readHeader(std::string_view text, bool skipControls)
{
  header_reader h{skipControls};
  rapidjson::Reader reader;
  rapidjson::MemoryStream ms{text.data(), text.size()};
  rapidjson::EncodedInputStream<rapidjson::UTF8<>, rapidjson::MemoryStream> is{ms};

  reader.Parse<rapidjson::kParseStopWhenDoneFlag>(is, h);

  if(!h.done || h.format != format_id)
    return std::nullopt;
  return h.out;
}
}

std::optional<DeviceHeader> parseDeviceMapHeader(std::string_view text)
{
  // The cheap read stops at the first control, which is everything a document
  // that states its own fields first needs. One that states them after its
  // controls is read by walking past them: rarer, and the whole text, but a
  // document is not allowed to be invisible for the order it chose.
  if(auto fast = readHeader(text, /*skipControls=*/false))
    return fast;
  return readHeader(text, /*skipControls=*/true);
}

std::string DeviceMap::label() const
{
  if(manufacturer.empty())
    return model;
  if(model.empty())
    return manufacturer;
  return manufacturer + ": " + model;
}

std::pair<int, int> naturalRange(MessageType t) noexcept
{
  switch(t)
  {
    case MessageType::CC14:
    case MessageType::NRPN:
    case MessageType::RPN:
    case MessageType::PitchBend:
      return {0, 16383};
    default:
      return {0, 127};
  }
}

std::pair<int, int> resolvedRange(const Value& v, MessageType t) noexcept
{
  const auto [lo, hi] = naturalRange(t);
  return {v.min.value_or(lo), v.max.value_or(hi)};
}

int decodeRelative(int byte, Encoding e) noexcept
{
  const int b = byte & 0x7F;
  switch(e)
  {
    case Encoding::TwosComplement:
      return b >= 64 ? b - 128 : b;
    case Encoding::SignedBit:
      return (b & 0x40) ? -(b & 0x3F) : b;
    case Encoding::SignedBit2:
      return (b & 0x40) ? (b & 0x3F) : -(b & 0x3F);
    case Encoding::BinaryOffset:
      return b - 64;
  }
  return 0;
}

int encodeRelative(int delta, Encoding e) noexcept
{
  switch(e)
  {
    case Encoding::TwosComplement:
      delta = std::clamp(delta, -64, 63);
      return delta < 0 ? delta + 128 : delta;
    case Encoding::SignedBit:
      delta = std::clamp(delta, -63, 63);
      return delta < 0 ? (0x40 | -delta) : delta;
    case Encoding::SignedBit2:
      delta = std::clamp(delta, -63, 63);
      return delta < 0 ? -delta : (0x40 | delta);
    case Encoding::BinaryOffset:
      delta = std::clamp(delta, -64, 63);
      return delta + 64;
  }
  return 0;
}

std::optional<DeviceMap> parseDeviceMap(std::string_view text)
{
  rapidjson::Document doc;
  doc.Parse(text.data(), text.size());
  if(doc.HasParseError() || !doc.IsObject())
    return std::nullopt;

  if(str(doc, "format") != format_id)
    return std::nullopt;

  DeviceMap map;
  map.manufacturer = str(doc, "manufacturer");
  map.model = str(doc, "model");
  map.description = str(doc, "description");
  map.requirement = str(doc, "requires");

  if(const auto* p = member(doc, "preset"))
    map.preset = parsePreset(*p);
  if(const auto* s = member(doc, "source"))
    map.source = parseSource(*s);

  if(const auto* m = member(doc, "match"); m && m->IsArray())
    for(const auto& s : m->GetArray())
      if(s.IsString())
        map.match.emplace_back(s.GetString(), s.GetStringLength());

  const auto* controls = member(doc, "controls");
  if(!controls || !controls->IsArray())
    return map;

  int index = 0;
  for(const auto& c : controls->GetArray())
  {
    std::string why;
    if(auto parsed = parseControl(c, why))
      map.controls.push_back(std::move(*parsed));
    else
      map.warnings.push_back("control " + std::to_string(index) + ": " + why);
    ++index;
  }

  return map;
}

std::string_view toString(Kind k) noexcept
{
  for(const auto& [name, v] : kinds)
    if(v == k)
      return name;
  return "other";
}

std::string_view toString(MessageType t) noexcept
{
  for(const auto& [name, v] : message_types)
    if(v == t)
      return name;
  return "cc";
}

}
