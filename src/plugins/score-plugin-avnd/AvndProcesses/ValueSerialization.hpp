#pragma once
#include <ossia/detail/json.hpp>
#include <ossia/network/value/format_value.hpp>
#include <ossia/network/value/value.hpp>

#include <QCborStreamReader>
#include <QCborStreamWriter>
#include <QIODevice>

#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <rapidjson/error/en.h>
#include <rapidjson/memorystream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/reader.h>

#include <cmath>

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <climits>
#include <cstdint>
#include <iterator>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace avnd_tools::value_serialization
{
// Deliberately ordinary JSON/CBOR, not OSCQuery's typetag-dependent wire format.
// null <-> impulse; vectors -> arrays -> lists; unset values are errors.
// Integer tokens must fit int32. Floating tokens round to float32, with overflow,
// non-finite values and nonzero-to-zero underflow rejected. This is NOT a lossless
// int64/double archive. UTF-8 text and embedded NULs are length-aware throughout.
// Objects/maps preserve insertion order and reject duplicate keys. CBOR accepts
// the JSON-compatible subset only: text keys, no tags/byte strings/undefined.
// Bounded event-time work: 8 MiB wire data, 65536 values+keys, 64 containers.
// JSON is strict (including UTF-8), overriding libossia's permissive parse flags.
inline constexpr std::size_t max_bytes = 8 * 1024 * 1024;
inline constexpr std::size_t max_nodes = 65536;
inline constexpr std::size_t max_depth = 64;

struct codec_error : std::runtime_error
{
  using std::runtime_error::runtime_error;
};

struct limits
{
  std::size_t nodes{};
  void node()
  {
    if(++nodes > max_nodes)
      throw codec_error{"Value count limit exceeded"};
  }
  static void depth(std::size_t n)
  {
    if(n > max_depth)
      throw codec_error{"Nesting depth limit exceeded"};
  }
};

inline void utf8(std::string_view text)
{
  rapidjson::MemoryStream stream{text.data(), text.size()};
  while(stream.Tell() < text.size())
  {
    unsigned codepoint{};
    if(!rapidjson::UTF8<>::Decode(stream, &codepoint))
      throw codec_error{"Invalid UTF-8 text"};
  }
}

inline ossia::value floating(double number)
{
  if(!std::isfinite(number) || std::abs(number) > std::numeric_limits<float>::max())
    throw codec_error{"Number outside finite float32 range"};
  const float value = static_cast<float>(number);
  if(number != 0. && value == 0.f)
    throw codec_error{"Number underflows float32"};
  return value;
}

inline ossia::value integer(std::int64_t number)
{
  if(number < std::numeric_limits<std::int32_t>::min()
     || number > std::numeric_limits<std::int32_t>::max())
    throw codec_error{"Integer outside int32 range"};
  return static_cast<std::int32_t>(number);
}

inline ossia::value unsigned_integer(std::uint64_t number)
{
  if(number > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
    throw codec_error{"Integer outside int32 range"};
  return static_cast<std::int32_t>(number);
}

// SAX builds the destination directly: no unbounded DOM or recursive parser.
struct json_reader : rapidjson::BaseReaderHandler<rapidjson::UTF8<>, json_reader>
{
  struct frame
  {
    ossia::value value;
    std::string key;
    std::unordered_set<std::string> keys;
  };
  std::vector<frame> stack;
  ossia::value result;
  limits budget;

  bool put(ossia::value value)
  {
    if(stack.empty())
      result = std::move(value);
    else if(auto* list = stack.back().value.target<std::vector<ossia::value>>())
      list->push_back(std::move(value));
    else
      stack.back().value.get<ossia::value_map_type>().emplace_back(
          std::move(stack.back().key), std::move(value));
    return true;
  }
  bool scalar(ossia::value value)
  {
    budget.node();
    return put(std::move(value));
  }
  bool Null() { return scalar(ossia::impulse{}); }
  bool Bool(bool v) { return scalar(v); }
  bool RawNumber(const char* s, rapidjson::SizeType n, bool)
  {
    const std::string_view token{s, n};
    if(token.find_first_of(".eE") == std::string_view::npos)
    {
      std::int32_t number{};
      const auto parsed = std::from_chars(s, s + n, number);
      if(parsed.ec != std::errc{} || parsed.ptr != s + n)
        throw codec_error{"Integer outside int32 range"};
      return scalar(number);
    }
    double number{};
    const auto parsed = std::from_chars(s, s + n, number);
    if(parsed.ec != std::errc{} || parsed.ptr != s + n)
      throw codec_error{"Number outside finite float32 range"};
    return scalar(floating(number));
  }
  bool String(const char* s, rapidjson::SizeType n, bool)
  {
    return scalar(std::string{s, n});
  }
  bool Key(const char* s, rapidjson::SizeType n, bool)
  {
    budget.node();
    auto& top = stack.back();
    top.key.assign(s, n);
    if(!top.keys.insert(top.key).second)
      throw codec_error{"Duplicate map key"};
    return true;
  }
  bool start(ossia::value value)
  {
    budget.node();
    limits::depth(stack.size() + 1);
    stack.push_back({std::move(value), {}, {}});
    return true;
  }
  bool StartObject() { return start(ossia::value_map_type{}); }
  bool StartArray() { return start(std::vector<ossia::value>{}); }
  bool end()
  {
    auto value = std::move(stack.back().value);
    stack.pop_back();
    return put(std::move(value));
  }
  bool EndObject(rapidjson::SizeType) { return end(); }
  bool EndArray(rapidjson::SizeType) { return end(); }
};

inline ossia::value from_json(std::string_view input)
{
  if(input.size() > max_bytes)
    throw codec_error{"Input byte limit exceeded"};
  // MemoryStream's EOF sentinel is NUL: forbid actual NUL bytes in JSON text,
  // otherwise a valid root followed by NUL and junk could appear complete.
  if(input.find('\0') != std::string_view::npos)
    throw codec_error{"Unescaped NUL in JSON text"};
  rapidjson::MemoryStream stream{input.data(), input.size()};
  rapidjson::Reader reader;
  json_reader handler;
  constexpr unsigned flags = rapidjson::kParseValidateEncodingFlag
                             | rapidjson::kParseNumbersAsStringsFlag
                             | rapidjson::kParseIterativeFlag;
  if(!reader.Parse<flags>(stream, handler))
    throw codec_error{
        std::string{rapidjson::GetParseError_En(reader.GetParseErrorCode())}
        + " at byte " + std::to_string(reader.GetErrorOffset())};
  return std::move(handler.result);
}

// This stream bounds allocation during writing, rather than checking after a
// potentially enormous escaped JSON string has already been allocated.
struct json_output
{
  using Ch = char;
  std::string data;
  void Put(char c)
  {
    if(data.size() == max_bytes)
      throw codec_error{"Output byte limit exceeded"};
    data.push_back(c);
  }
  void Flush() { }
};

template <template <typename, typename, typename, typename, unsigned> class Writer>
struct basic_json_writer
{
  json_output output;
  Writer<
      json_output, rapidjson::UTF8<>, rapidjson::UTF8<>, rapidjson::CrtAllocator,
      rapidjson::kWriteValidateEncodingFlag>
      writer{output};
  void null() { writer.Null(); }
  void number(int v) { writer.Int(v); }
  void number(float v) { writer.Double(v); }
  void boolean(bool v) { writer.Bool(v); }
  void text(std::string_view v)
  {
    if(!writer.String(v.data(), static_cast<rapidjson::SizeType>(v.size())))
      throw codec_error{"Invalid UTF-8 text"};
  }
  void key(std::string_view v) { text(v); }
  void array(std::size_t) { writer.StartArray(); }
  void map(std::size_t) { writer.StartObject(); }
  void end_array() { writer.EndArray(); }
  void end_map() { writer.EndObject(); }
};

using json_writer = basic_json_writer<rapidjson::Writer>;
using pretty_json_writer = basic_json_writer<rapidjson::PrettyWriter>;

struct cbor_output : QIODevice
{
  std::string data;
  bool overflow{};
  cbor_output() { open(QIODevice::WriteOnly); }
  qint64 readData(char*, qint64) override { return -1; }
  qint64 writeData(const char* source, qint64 size) override
  {
    if(size < 0 || static_cast<std::uint64_t>(size) > max_bytes - data.size())
    {
      overflow = true;
      return -1;
    }
    data.append(source, static_cast<std::size_t>(size));
    return size;
  }
};

struct cbor_writer
{
  cbor_output output;
  QCborStreamWriter writer{&output};
  void null() { writer.appendNull(); }
  void number(int v) { writer.append(v); }
  void number(float v) { writer.append(v); }
  void boolean(bool v) { writer.append(v); }
  void text(std::string_view v)
  {
    utf8(v);
    writer.appendTextString(v.data(), static_cast<qsizetype>(v.size()));
  }
  void key(std::string_view v) { text(v); }
  void array(std::size_t n) { writer.startArray(n); }
  void map(std::size_t n) { writer.startMap(n); }
  void end_array() { writer.endArray(); }
  void end_map() { writer.endMap(); }
};

template <typename Writer>
struct value_writer
{
  Writer& writer;
  limits& budget;
  std::size_t depth{};
  void operator()() const { throw codec_error{"Cannot serialize an unset value"}; }
  void operator()(ossia::impulse) const { writer.null(); }
  void operator()(int v) const { writer.number(v); }
  void operator()(float v) const
  {
    if(!std::isfinite(v))
      throw codec_error{"Cannot serialize a non-finite number"};
    writer.number(v);
  }
  void operator()(bool v) const { writer.boolean(v); }
  void operator()(const std::string& v) const
  {
    if(v.size() > max_bytes)
      throw codec_error{"String byte limit exceeded"};
    writer.text(v);
  }
  void child(const ossia::value& v) const
  {
    budget.node();
    v.apply(value_writer{writer, budget, depth + 1});
    if constexpr(requires { writer.output.overflow; })
      if(writer.output.overflow)
        throw codec_error{"Output byte limit exceeded"};
  }
  template <std::size_t N>
  void operator()(const std::array<float, N>& v) const
  {
    limits::depth(depth + 1);
    writer.array(N);
    for(float x : v)
    {
      budget.node();
      (*this)(x);
    }
    writer.end_array();
  }
  void operator()(const std::vector<ossia::value>& v) const
  {
    limits::depth(depth + 1);
    writer.array(v.size());
    for(const auto& x : v)
      child(x);
    writer.end_array();
  }
  void operator()(const ossia::value_map_type& v) const
  {
    limits::depth(depth + 1);
    writer.map(v.size());
    std::unordered_set<std::string_view> keys;
    for(const auto& [key, value] : v)
    {
      budget.node();
      if(key.size() > max_bytes)
        throw codec_error{"Key byte limit exceeded"};
      if(!keys.insert(key).second)
        throw codec_error{"Duplicate map key"};
      writer.key(key);
      child(value);
    }
    writer.end_map();
  }
};

template <typename Writer>
std::string encode(const ossia::value& value)
{
  Writer writer;
  limits budget;
  budget.node();
  value.apply(value_writer<Writer>{writer, budget});
  if constexpr(requires { writer.output.overflow; })
    if(writer.output.overflow)
      throw codec_error{"Output byte limit exceeded"};
  return std::move(writer.output.data);
}

inline std::string cbor_text(QCborStreamReader& reader)
{
  std::string text;
  for(;;)
  {
    // Bound advertised chunk sizes BEFORE allocating; consume directly into
    // UTF-8 storage without a QString round trip.
    const auto size = reader.currentStringChunkSize();
    if(size < 0 || static_cast<std::uint64_t>(size) > max_bytes - text.size())
      throw codec_error{"String byte limit exceeded"};
    const auto offset = text.size();
    text.resize(offset + static_cast<std::size_t>(size));
    const auto chunk = reader.readStringChunk(text.data() + offset, size);
    if(chunk.status == QCborStreamReader::Error)
      throw codec_error{"Malformed CBOR text"};
    text.resize(offset + static_cast<std::size_t>(chunk.data));
    if(chunk.status == QCborStreamReader::EndOfString)
      return text;
    // CBOR requires each chunk (not merely their concatenation) to be UTF-8.
    utf8(std::string_view{text}.substr(offset));
  }
}

inline ossia::value
cbor_value(QCborStreamReader& reader, limits& budget, std::size_t depth = 0)
{
  budget.node();
  ossia::value result;
  if(reader.isContainer())
  {
    limits::depth(depth + 1);
    const bool map = reader.isMap();
    if(reader.isLengthKnown() && reader.length() > max_nodes)
      throw codec_error{"Value count limit exceeded"};
    if(!reader.enterContainer())
      throw codec_error{"Malformed CBOR container"};
    if(map)
    {
      ossia::value_map_type values;
      std::unordered_set<std::string> keys;
      while(reader.hasNext())
      {
        budget.node();
        if(!reader.isString())
          throw codec_error{"CBOR map keys must be text"};
        auto key = cbor_text(reader);
        if(!keys.insert(key).second)
          throw codec_error{"Duplicate map key"};
        values.emplace_back(std::move(key), cbor_value(reader, budget, depth + 1));
      }
      result = std::move(values);
    }
    else
    {
      std::vector<ossia::value> values;
      while(reader.hasNext())
        values.push_back(cbor_value(reader, budget, depth + 1));
      result = std::move(values);
    }
    if(!reader.leaveContainer())
      throw codec_error{"Malformed CBOR container"};
    return result;
  }
  if(reader.isString())
    return cbor_text(reader);
  if(reader.isUnsignedInteger())
    result = unsigned_integer(reader.toUnsignedInteger());
  else if(reader.isNegativeInteger())
  {
    // QCborNegativeInteger stores the absolute magnitude, with 0 meaning 2^64.
    const auto magnitude = static_cast<quint64>(reader.toNegativeInteger());
    if(magnitude == 0 || magnitude > 2147483648ULL)
      throw codec_error{"Integer outside int32 range"};
    result = integer(-static_cast<std::int64_t>(magnitude));
  }
  else if(reader.isBool())
    result = reader.toBool();
  else if(reader.isNull())
    result = ossia::impulse{};
  else if(reader.isFloat16())
    result = floating(static_cast<float>(reader.toFloat16()));
  else if(reader.isFloat())
    result = floating(reader.toFloat());
  else if(reader.isDouble())
    result = floating(reader.toDouble());
  else
    throw codec_error{
        "Unsupported or malformed CBOR value (tags, bytes and undefined are not "
        "values)"};
  if(!reader.next())
    throw codec_error{"Malformed CBOR value"};
  return result;
}

inline ossia::value from_cbor(std::string_view input)
{
  if(input.size() > max_bytes)
    throw codec_error{"Input byte limit exceeded"};
  QCborStreamReader reader{input.data(), static_cast<qsizetype>(input.size())};
  limits budget;
  auto result = cbor_value(reader, budget);
  if(reader.lastError() != QCborError::NoError)
    throw codec_error{"Malformed CBOR input"};
  if(reader.currentOffset() != static_cast<qint64>(input.size()))
    throw codec_error{"Trailing CBOR input"};
  return result;
}

enum class Format
{
  JSON,
  CBOR,
  Text,
  Binary
};
enum class TextStyle
{
  Plain,
  Pretty
};
enum class TextType
{
  Auto,
  String,
  Integer,
  Float,
  Boolean,
  List
};
enum class LineEnding
{
  None,
  LF,
  CRLF
};
enum class ByteOrder
{
  Little,
  Big
};
enum class BinaryMode
{
  FreeFlow,
  Layout
};
enum class ScalarType
{
  u8,
  i8,
  u16,
  i16,
  u32,
  i32,
  f32,
  f64
};

inline void append_bytes(std::string& output, std::string_view bytes)
{
  if(bytes.size() > max_bytes - output.size())
    throw codec_error{"Output byte limit exceeded"};
  output.append(bytes);
}

inline std::string_view line_ending(LineEnding ending)
{
  switch(ending)
  {
    case LineEnding::LF:
      return "\n";
    case LineEnding::CRLF:
      return "\r\n";
    default:
      return {};
  }
}

// Plain text infers shape: root strings are raw, flat lists/vectors delimited
// JSON tokens, nested containers JSON. String and List decoding resolve the
// unavoidable root-string and one-element-list ambiguities.
inline ossia::value text_scalar(std::string_view text, TextType type)
{
  if(type == TextType::String)
    return std::string{text};
  if(type == TextType::Boolean || type == TextType::Auto)
  {
    if(text == "true")
      return true;
    if(text == "false")
      return false;
    if(type == TextType::Boolean)
      throw codec_error{"Expected true or false"};
  }
  if(type == TextType::Integer
     || (type == TextType::Auto && text.find_first_of(".eE") == text.npos))
  {
    std::int32_t number{};
    auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
    if(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size())
      return number;
    if(type == TextType::Integer || parsed.ec == std::errc::result_out_of_range)
      throw codec_error{"Expected int32 text"};
  }
  double number{};
  auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
  if(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size())
    return floating(number);
  const bool numeric
      = !text.empty()
        && ((text.front() >= '0' && text.front() <= '9') || text.front() == '-'
            || text.front() == '+' || text.front() == '.');
  if(type == TextType::Float || numeric)
    throw codec_error{"Invalid or out-of-range numeric text"};
  return std::string{text};
}

inline bool structured_text(std::string_view text)
{
  const auto start = text.find_first_not_of(" \t\r\n");
  if(start == text.npos)
    return false;
  text.remove_prefix(start);
  const char first = text.front();
  return first == '[' || first == '{' || first == '"' || first == '-'
         || (first >= '0' && first <= '9') || text == "true" || text == "false"
         || text == "null";
}

// Use the very same formatter as value_to_pretty_string, but with a bounded
// output iterator: the public helper allocates an unbounded intermediate string.
struct pretty_output_iterator
{
  using iterator_category = std::output_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = void;
  using pointer = void;
  using reference = void;
  json_output* output;
  pretty_output_iterator& operator*() { return *this; }
  pretty_output_iterator& operator++() { return *this; }
  pretty_output_iterator operator++(int) { return *this; }
  pretty_output_iterator& operator=(char c)
  {
    output->Put(c);
    return *this;
  }
};

struct validation_writer
{
  void null() { }
  void number(int) { }
  void number(float) { }
  void boolean(bool) { }
  void text(std::string_view text) { utf8(text); }
  void key(std::string_view text) { utf8(text); }
  void array(std::size_t) { }
  void map(std::size_t) { }
  void end_array() { }
  void end_map() { }
};

inline void validate_value(const ossia::value& value)
{
  validation_writer writer;
  limits budget;
  budget.node();
  value.apply(value_writer<validation_writer>{writer, budget});
}

inline std::string to_pretty(const ossia::value& value)
{
  validate_value(value);
  json_output bounded;
  fmt::format_to(pretty_output_iterator{&bounded}, "{}", value);
  return std::move(bounded.data);
}

inline ossia::value from_pretty(std::string_view text)
{
  // Bound recursion and allocation BEFORE entering libossia's recursive grammar.
  // Outside strings every value starts with a type name, and vector coordinates
  // start with a number. Counting both conservatively also bounds list storage.
  limits budget;
  std::size_t depth{};
  bool quoted{};
  for(std::size_t i = 0; i < text.size();)
  {
    const char c = text[i++];
    if(quoted)
    {
      if(c == '\\' && i < text.size() && text[i] == '"')
        ++i;
      else if(c == '"')
        quoted = false;
      continue;
    }
    if(c == '"')
      quoted = true;
    else if(c == '[')
      limits::depth(++depth);
    else if(c == ']')
    {
      if(depth == 0)
        throw codec_error{"Unmatched pretty container"};
      --depth;
    }
    else if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
    {
      budget.node();
      while(
          i < text.size()
          && ((text[i] >= 'a' && text[i] <= 'z') || (text[i] >= '0' && text[i] <= '9')))
        ++i;
    }
    else if((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.')
    {
      budget.node();
      const auto start = i - 1;
      while(i < text.size()
            && ((text[i] >= '0' && text[i] <= '9') || text[i] == '.' || text[i] == 'e'
                || text[i] == 'E' || text[i] == '+' || text[i] == '-'))
        ++i;
      double number{};
      const auto token = text.substr(start, i - start);
      const auto parsed
          = std::from_chars(token.data(), token.data() + token.size(), number);
      if(parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size())
        throw codec_error{"Invalid pretty number"};
      floating(number);
    }
  }
  if(quoted || depth)
    throw codec_error{"Unterminated pretty container or string"};
  // The public parser accepts trailing junk. Restrict decoding to canonical
  // formatter output so this cannot silently accept an incomplete payload.
  auto result = ossia::parse_pretty_value(text);
  if(!result.valid() || to_pretty(result) != text)
    throw codec_error{"Expected canonical ossia pretty text (maps are display only)"};
  return result;
}

inline void text_delimiter(std::string_view delimiter)
{
  if(delimiter.empty() || delimiter.size() > max_bytes
     || delimiter.find_first_of("\"\\[]{}") != delimiter.npos)
    throw codec_error{
        "Delimiter must be nonempty, bounded and exclude JSON quotes, escapes and "
        "brackets"};
  utf8(delimiter);
}

inline std::size_t text_separator(std::string_view text, std::string_view delimiter)
{
  bool quoted{};
  for(std::size_t i = 0; i < text.size(); ++i)
  {
    if(quoted && text[i] == '\\')
      ++i;
    else if(text[i] == '"')
      quoted = !quoted;
    else if(!quoted && text.substr(i).starts_with(delimiter))
      return i;
  }
  return text.npos;
}

template <typename Values>
std::string delimited_text(const Values& values, std::string_view delimiter)
{
  text_delimiter(delimiter);
  if(values.size() >= max_nodes)
    throw codec_error{"Value count limit exceeded"};
  std::string output;
  std::size_t field_start{};
  bool first = true;
  for(const auto& value : values)
  {
    auto token = encode<json_writer>(value);
    if(text_separator(token, delimiter) != token.npos)
      throw codec_error{"Numeric text field contains the delimiter"};
    if(!first)
    {
      const auto boundary = output.size();
      append_bytes(output, delimiter);
      if(text_separator(std::string_view{output}.substr(field_start), delimiter)
         != boundary - field_start)
        throw codec_error{"Text field overlaps the delimiter"};
      field_start = output.size();
    }
    append_bytes(output, token);
    first = false;
  }
  return output;
}

inline ossia::value from_text(
    std::string_view text, TextStyle style, TextType type, std::string_view delimiter,
    LineEnding ending)
{
  if(text.size() > max_bytes)
    throw codec_error{"Input byte limit exceeded"};
  utf8(text);
  // String interpretation is a byte-for-byte UTF-8 escape hatch, even when
  // the payload looks like JSON or ends in the selected line ending.
  if(type == TextType::String)
    return std::string{text};
  const auto suffix = line_ending(ending);
  if(!suffix.empty() && text.ends_with(suffix))
    text.remove_suffix(suffix.size());
  if(style == TextStyle::Pretty)
    return from_pretty(text);
  if(type != TextType::Auto && type != TextType::List)
    return text_scalar(text, type);
  const auto start = text.find_first_not_of(" \t\r\n");
  if(start != text.npos && (text[start] == '[' || text[start] == '{'))
    return from_json(text);
  text_delimiter(delimiter);
  auto separator = text_separator(text, delimiter);
  if(type != TextType::List && separator == text.npos)
    return structured_text(text) ? from_json(text) : text_scalar(text, TextType::Auto);
  std::vector<ossia::value> values;
  limits budget;
  budget.node();
  for(;;)
  {
    budget.node();
    const auto token = text.substr(0, separator);
    const auto token_start = token.find_first_not_of(" \t\r\n");
    if(token_start != token.npos
       && (token[token_start] == '[' || token[token_start] == '{'))
      throw codec_error{"Nested text containers require a complete JSON representation"};
    values.push_back(
        structured_text(token) ? from_json(token) : text_scalar(token, TextType::Auto));
    if(separator == text.npos)
      break;
    text.remove_prefix(separator + delimiter.size());
    separator = text_separator(text, delimiter);
  }
  return values;
}

inline std::string to_text(
    const ossia::value& value, TextStyle style, std::string_view delimiter,
    LineEnding ending)
{
  if(const auto* list = value.target<std::vector<ossia::value>>();
     list && list->size() >= max_nodes)
    throw codec_error{"Value count limit exceeded"};
  std::string output;
  if(style == TextStyle::Pretty)
    output = to_pretty(value);
  else if(const auto* text = value.target<std::string>())
  {
    if(text->size() > max_bytes)
      throw codec_error{"String byte limit exceeded"};
    utf8(*text);
    // Raw strings have no added framing, preserving embedded NUL and endings.
    return *text;
  }
  else if(
      const auto* list = value.target<std::vector<ossia::value>>();
      list && !list->empty()
      && std::all_of(list->begin(), list->end(), [](const ossia::value& item) {
    return item.target<int>() || item.target<float>() || item.target<bool>()
           || item.target<std::string>() || item.target<ossia::impulse>();
  }))
    output = delimited_text(*list, delimiter);
  else if(const auto* vector = value.target<ossia::vec2f>())
    output = delimited_text(*vector, delimiter);
  else if(const auto* vector = value.target<ossia::vec3f>())
    output = delimited_text(*vector, delimiter);
  else if(const auto* vector = value.target<ossia::vec4f>())
    output = delimited_text(*vector, delimiter);
  else
    output = encode<json_writer>(value);
  append_bytes(output, line_ending(ending));
  return output;
}

// Strict subset of Python struct. No prefix: selected endian, packed.
// < little, > or ! big, = native endian packed, @ native endian/alignment.
// [count]b B h H i I q Q f d ? s x, with whitespace only between fields.
// Ns is ONE exact-size raw string (never silently padded or truncated).
// Nx is explicit zero padding; zero repeats are numeric alignment directives.
// Native ABI requires the standard widths below; no automatic trailing padding.
// Decoding returns a flat list: integer results must fit int32 and floating
// results round to finite float32 without overflow or nonzero-to-zero underflow.
inline std::size_t binary_width(char code)
{
  switch(code)
  {
    case 'b':
    case 'B':
    case '?':
    case 's':
    case 'x':
      return 1;
    case 'h':
    case 'H':
      return 2;
    case 'i':
    case 'I':
    case 'f':
      return 4;
    case 'q':
    case 'Q':
    case 'd':
      return 8;
    default:
      throw codec_error{"Unknown binary format code"};
  }
}

inline char scalar_code(ScalarType type)
{
  switch(type)
  {
    case ScalarType::u8:
      return 'B';
    case ScalarType::i8:
      return 'b';
    case ScalarType::u16:
      return 'H';
    case ScalarType::i16:
      return 'h';
    case ScalarType::u32:
      return 'I';
    case ScalarType::i32:
      return 'i';
    case ScalarType::f32:
      return 'f';
    case ScalarType::f64:
      return 'd';
  }
  throw codec_error{"Unknown scalar type"};
}

struct binary_field
{
  char code;
  std::size_t count;
};
struct binary_layout
{
  ByteOrder order{};
  std::vector<binary_field> fields;
  std::size_t bytes{};
  std::size_t values{};
};

static_assert(
    CHAR_BIT == 8 && sizeof(short) == 2 && sizeof(int) == 4 && sizeof(long long) == 8
    && sizeof(bool) == 1 && sizeof(float) == 4 && sizeof(double) == 8
    && std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559);
static_assert(
    std::endian::native == std::endian::little
    || std::endian::native == std::endian::big);

inline std::size_t native_alignment(char code)
{
  switch(code)
  {
    case 'h':
    case 'H':
      return alignof(short);
    case 'i':
    case 'I':
      return alignof(int);
    case 'q':
    case 'Q':
      return alignof(long long);
    case 'f':
      return alignof(float);
    case 'd':
      return alignof(double);
    case '?':
      return alignof(bool);
    default:
      return 1;
  }
}

inline binary_layout compile_layout(std::string_view format, ByteOrder order)
{
  if(format.size() > max_nodes)
    throw codec_error{"Format text limit exceeded"};
  binary_layout layout{order, {}, 0, 0};
  auto whitespace
      = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
  std::size_t pos{};
  while(pos < format.size() && whitespace(format[pos]))
    ++pos;
  bool native{};
  if(pos < format.size())
  {
    const char prefix = format[pos];
    if(prefix == '<' || prefix == '>' || prefix == '!' || prefix == '=' || prefix == '@')
    {
      ++pos;
      native = prefix == '@';
      layout.order = prefix == '<'                                ? ByteOrder::Little
                     : prefix == '>' || prefix == '!'             ? ByteOrder::Big
                     : std::endian::native == std::endian::little ? ByteOrder::Little
                                                                  : ByteOrder::Big;
    }
  }
  while(pos < format.size())
  {
    if(whitespace(format[pos]))
    {
      ++pos;
      continue;
    }
    std::size_t count{};
    bool explicit_count{};
    while(pos < format.size() && format[pos] >= '0' && format[pos] <= '9')
    {
      explicit_count = true;
      count = count * 10 + (format[pos++] - '0');
      if(count > max_nodes)
        throw codec_error{"Binary count limit exceeded"};
    }
    if(!explicit_count)
      count = 1;
    if(pos == format.size())
      throw codec_error{"Invalid binary count"};
    const char code = format[pos++];
    const auto width = binary_width(code);
    if(count == 0 && (code == 's' || code == 'x'))
      throw codec_error{"Zero repeats require a numeric alignment code"};
    const auto alignment = native ? native_alignment(code) : 1;
    const auto padding = (alignment - layout.bytes % alignment) % alignment;
    if(padding > max_bytes - layout.bytes)
      throw codec_error{"Binary layout resource limit exceeded"};
    if(padding)
    {
      layout.fields.push_back({'x', padding});
      layout.bytes += padding;
    }
    const auto bytes = count * width;
    const auto values = code == 'x' ? 0 : code == 's' ? 1 : count;
    if(bytes > max_bytes - layout.bytes || values >= max_nodes - layout.values)
      throw codec_error{"Binary layout resource limit exceeded"};
    layout.bytes += bytes;
    layout.values += values;
    layout.fields.push_back({code, count});
  }
  if(layout.fields.empty())
    throw codec_error{"Empty binary layout"};
  return layout;
}

struct layout_cache
{
  std::string source;
  ByteOrder order{};
  bool initialized{};
  binary_layout layout;
  std::string error;
  const binary_layout& get(std::string_view format, ByteOrder requested)
  {
    if(format.size() > max_nodes)
      throw codec_error{"Format text limit exceeded"};
    if(!initialized || source != format || order != requested)
    {
      source = format;
      order = requested;
      initialized = true;
      error.clear();
      try
      {
        layout = compile_layout(format, requested);
      }
      catch(const codec_error& e)
      {
        error = e.what();
      }
    }
    if(!error.empty())
      throw codec_error{error};
    return layout;
  }
};

inline void
write_word(std::string& output, std::uint64_t word, std::size_t width, ByteOrder order)
{
  if(width > max_bytes - output.size())
    throw codec_error{"Output byte limit exceeded"};
  for(std::size_t i = 0; i < width; ++i)
  {
    const auto shift = 8 * (order == ByteOrder::Little ? i : width - 1 - i);
    output.push_back(static_cast<char>((word >> shift) & 255));
  }
}

inline std::uint64_t
read_word(std::string_view input, std::size_t& pos, std::size_t width, ByteOrder order)
{
  if(width > input.size() - pos)
    throw codec_error{"Truncated binary input"};
  std::uint64_t word{};
  for(std::size_t i = 0; i < width; ++i)
  {
    const auto shift = 8 * (order == ByteOrder::Little ? i : width - 1 - i);
    word |= std::uint64_t(static_cast<unsigned char>(input[pos++])) << shift;
  }
  return word;
}

inline ossia::value
read_scalar(std::string_view input, std::size_t& pos, char code, ByteOrder order)
{
  const auto width = binary_width(code);
  const auto word = read_word(input, pos, width, order);
  switch(code)
  {
    case 'f':
      return floating(std::bit_cast<float>(static_cast<std::uint32_t>(word)));
    case 'd':
      return floating(std::bit_cast<double>(word));
    case '?':
      if(word > 1)
        throw codec_error{"Boolean byte must be 0 or 1"};
      return bool(word);
    case 'b':
    case 'h':
    case 'i':
    case 'q': {
      const auto extended = width < 8 && (word & (std::uint64_t{1} << (width * 8 - 1)))
                                ? word | (~std::uint64_t{0} << (width * 8))
                                : word;
      return integer(std::bit_cast<std::int64_t>(extended));
    }
    default:
      return unsigned_integer(word);
  }
}

inline void
write_scalar(std::string& output, const ossia::value& value, char code, ByteOrder order)
{
  const auto width = binary_width(code);
  if(code == 'f' || code == 'd')
  {
    double number;
    if(const auto* f = value.target<float>())
      number = *f;
    else if(const auto* i = value.target<int>())
      number = *i;
    else
      throw codec_error{"Floating field requires a number"};
    floating(number);
    const auto word
        = code == 'f'
              ? std::uint64_t(std::bit_cast<std::uint32_t>(static_cast<float>(number)))
              : std::bit_cast<std::uint64_t>(number);
    write_word(output, word, width, order);
    return;
  }
  std::int64_t number;
  if(const auto* i = value.target<int>())
    number = *i;
  else if(const auto* b = value.target<bool>())
    number = *b;
  else
    throw codec_error{"Integer field requires an integer or boolean"};
  const bool signed_type = code == 'b' || code == 'h' || code == 'i' || code == 'q';
  if(code == '?')
  {
    if(number < 0 || number > 1)
      throw codec_error{"Boolean field must be 0 or 1"};
  }
  else if(signed_type && width < 8)
  {
    const std::int64_t bound = std::int64_t{1} << (width * 8 - 1);
    if(number < -bound || number >= bound)
      throw codec_error{"Signed field overflow"};
  }
  else if(!signed_type)
  {
    if(number < 0
       || (width < 8 && std::uint64_t(number) >= (std::uint64_t{1} << (width * 8))))
      throw codec_error{"Unsigned field overflow"};
  }
  write_word(output, static_cast<std::uint64_t>(number), width, order);
}

struct free_binary_writer
{
  std::string& output;
  ByteOrder order;
  limits& budget;
  std::size_t depth{};
  void operator()() const { throw codec_error{"Cannot serialize an unset value"}; }
  void operator()(ossia::impulse) const
  {
    throw codec_error{"Binary impulse has no width"};
  }
  void operator()(int v) const { write_scalar(output, v, 'i', order); }
  void operator()(float v) const { write_scalar(output, v, 'f', order); }
  void operator()(bool v) const { write_scalar(output, v, '?', order); }
  void operator()(const std::string& v) const { append_bytes(output, v); }
  void operator()(const ossia::value_map_type&) const
  {
    throw codec_error{"Binary maps have ambiguous field order"};
  }
  void operator()(const std::vector<ossia::value>& values) const
  {
    limits::depth(depth + 1);
    for(const auto& value : values)
    {
      budget.node();
      value.apply(free_binary_writer{output, order, budget, depth + 1});
    }
  }
  template <std::size_t N>
  void operator()(const std::array<float, N>& values) const
  {
    limits::depth(depth + 1);
    for(float value : values)
    {
      budget.node();
      (*this)(value);
    }
  }
};

template <typename Values>
std::string to_binary_fields(const Values& values, const binary_layout& layout)
{
  std::string output;
  if(values.size() != layout.values)
    throw codec_error{"Binary layout input field count mismatch"};
  output.reserve(layout.bytes);
  std::size_t index{};
  for(const auto& field : layout.fields)
  {
    if(field.code == 'x')
    {
      output.append(field.count, '\0');
      continue;
    }
    const auto repeats = field.code == 's' ? 1 : field.count;
    for(std::size_t i = 0; i < repeats; ++i)
    {
      const ossia::value& item = values[index++];
      if(field.code == 's')
      {
        const auto* text = item.target<std::string>();
        if(!text || text->size() != field.count)
          throw codec_error{"Fixed string requires exactly its declared byte count"};
        append_bytes(output, *text);
      }
      else
        write_scalar(output, item, field.code, layout.order);
    }
  }
  return output;
}

inline std::string
to_binary(const ossia::value& value, ByteOrder order, const binary_layout* layout)
{
  if(!layout)
  {
    std::string output;
    limits budget;
    budget.node();
    value.apply(free_binary_writer{output, order, budget});
    return output;
  }
  if(const auto* list = value.target<std::vector<ossia::value>>())
    return to_binary_fields(*list, *layout);
  if(const auto* vector = value.target<ossia::vec2f>())
    return to_binary_fields(*vector, *layout);
  if(const auto* vector = value.target<ossia::vec3f>())
    return to_binary_fields(*vector, *layout);
  if(const auto* vector = value.target<ossia::vec4f>())
    return to_binary_fields(*vector, *layout);
  return to_binary_fields(std::span{&value, 1}, *layout);
}

inline ossia::value from_binary(
    std::string_view input, ByteOrder order, ScalarType type,
    const binary_layout* layout)
{
  if(input.size() > max_bytes)
    throw codec_error{"Input byte limit exceeded"};
  std::vector<ossia::value> values;
  std::size_t pos{};
  if(!layout)
  {
    const auto code = scalar_code(type);
    const auto width = binary_width(code);
    if(input.size() % width)
      throw codec_error{"Truncated binary scalar"};
    if(input.size() / width >= max_nodes)
      throw codec_error{"Value count limit exceeded"};
    values.reserve(input.size() / width);
    while(pos < input.size())
      values.push_back(read_scalar(input, pos, code, order));
  }
  else
  {
    if(input.size() != layout->bytes)
      throw codec_error{"Truncated or trailing binary input"};
    values.reserve(layout->values);
    for(const auto& field : layout->fields)
    {
      if(field.code == 'x')
      {
        pos += field.count;
        continue;
      }
      if(field.code == 's')
      {
        values.emplace_back(std::string{input.substr(pos, field.count)});
        pos += field.count;
      }
      else
        for(std::size_t i = 0; i < field.count; ++i)
          values.push_back(read_scalar(input, pos, field.code, layout->order));
    }
  }
  return values;
}

// Callback outputs emit once per input update, including repeated equal events.
// Each failed update emits an explicit empty/unset result and Success=false,
// never the previous successful payload. Error is empty on successful updates.
// Conversion finishes before publishing; callback order is error, status, data.
template <typename Output, typename Function>
void publish(Output& output, Function&& function)
{
  using result_type = decltype(function());
  result_type result;
  std::string error;
  try
  {
    result = function();
  }
  catch(const std::exception& e)
  {
    error = e.what();
  }
  output.error(error);
  output.success(error.empty());
  output.result(std::move(result));
}
}

namespace avnd_tools
{
struct Deserialize
{
  halp_meta(name, "Deserialize")
  halp_meta(c_name, "avnd_deserialize")
  halp_meta(author, "ossia team")
  halp_meta(category, "Control/Serialization")
  halp_meta(uuid, "5d265cd1-014f-48bb-9c51-d594c8b58b61")
  halp_meta(
      description,
      "Decode Bytes as strict JSON, JSON-compatible CBOR, UTF-8 text or binary. "
      "Integers must fit int32; floats round to finite float32 without underflow. "
      "Binary decoding returns an ordered list. Limits: 8 MiB, 65536 nodes, 64 "
      "containers. Errors clear Value; Error is empty on success.")
  struct ins
  {
    struct : halp::val_port<"Bytes", std::string>
    {
      void update(Deserialize& self) { self.process(); }
    } input;
    halp::enum_t<value_serialization::Format, "Format"> format;
    struct : halp::enum_t<value_serialization::TextStyle, "Text style">
    {
      halp_meta(
          description,
          "Plain reads delimited fields or nested JSON. Pretty reads canonical ossia "
          "type labels only; maps are display only, floats were rounded to two "
          "decimals, and only quotes are escaped.")
    } text_style;
    struct : halp::enum_t<value_serialization::TextType, "Interpretation">
    {
      halp_meta(
          description,
          "Auto recognizes separators, JSON and exact scalar tokens, otherwise raw "
          "strings. String preserves all UTF-8 bytes and line endings. List forces "
          "single delimited fields into a list. Numeric options require exact scalars. "
          "Pretty uses its own labels unless String is selected.")
    } text_type;
    struct : halp::lineedit<"Delimiter", ",">
    {
      halp_meta(
          description,
          "Separator for flat Plain lists and vectors, ignored inside JSON-quoted "
          "strings. Must exclude quotes, backslashes and brackets. Nested containers "
          "use JSON.")
    } delimiter;
    struct : halp::enum_t<value_serialization::LineEnding, "Line ending">
    {
      halp_meta(
          description,
          "Remove one matching suffix before parsing, except with String "
          "interpretation.")
    } line_ending;
    struct : halp::enum_t<value_serialization::ByteOrder, "Byte order">
    {
      halp_meta(
          description, "Raw scalar endian and the default for layouts without a prefix.")
    } byte_order;
    struct : halp::enum_t<value_serialization::BinaryMode, "Binary mode">
    {
      halp_meta(
          description,
          "FreeFlow reads repeated scalars; Layout reads explicitly typed fields. Both "
          "return lists.")
    } binary_mode;
    struct : halp::enum_t<value_serialization::ScalarType, "Scalar type">
    {
      halp_meta(
          description,
          "Repeated raw binary width. Integers must fit int32; floats round to finite "
          "float32 without underflow.")
    } scalar_type;
    struct : halp::lineedit<"Layout", "<B H I f 4s 2x">
    {
      halp_meta(
          description,
          "Python struct subset: < > ! = packed, @ native endian and alignment; "
          "no prefix uses Byte order and packed widths. [count]b B h H i I q Q f d ? s "
          "x. "
          "Counts <=65536; zero numeric counts align only. Ns is one exact-size raw "
          "string; Nx skips padding. No automatic end padding. Int32 and finite float32 "
          "results.")
    } layout;
  } inputs;
  struct
  {
    halp::callback<"Value", ossia::value> result;
    halp::callback<"Success", bool> success;
    halp::callback<"Error", std::string> error;
  } outputs;

  struct ui
  {
    halp_meta(layout, halp::layouts::vbox)
    halp::item<&ins::format> format;
    struct Settings
    {
      halp_meta(layout, halp::layouts::tabs)
      halp_flag(hide_tabs);
      static constexpr auto model = &ins::format;
      struct
      {
        halp_meta(name, "JSON")
        halp_meta(layout, halp::layouts::vbox)
        halp::label types{.text = "Strict UTF-8 JSON: int32, finite float32."};
        halp::label values{.text = "null = impulse; arrays = lists; ordered maps."};
        halp::label limits{.text = "8 MiB, 65536 values/keys, 64 containers."};
      } json;
      struct
      {
        halp_meta(name, "CBOR")
        halp_meta(layout, halp::layouts::vbox)
        halp::label types{.text = "JSON-compatible CBOR: int32, finite float32."};
        halp::label keys{.text = "UTF-8 text keys; no tags, byte strings or undefined."};
        halp::label values{.text = "null = impulse; arrays = lists; ordered maps."};
        halp::label limits{.text = "8 MiB, 65536 values/keys, 64 containers."};
      } cbor;
      struct
      {
        halp_meta(name, "Text")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::text_style> style;
        halp::item<&ins::text_type> type;
        halp::item<&ins::delimiter> delimiter;
        halp::item<&ins::line_ending> ending;
      } text;
      struct
      {
        halp_meta(name, "Binary")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::byte_order> order;
        halp::item<&ins::binary_mode> mode;
        halp::item<&ins::scalar_type> scalar;
        halp::item<&ins::layout> format_text;
      } binary;
    } settings;
  };

  value_serialization::layout_cache cache;
  void process()
  {
    using namespace value_serialization;
    publish(outputs, [&]() -> ossia::value {
      const auto& bytes = inputs.input.value;
      switch(inputs.format.value)
      {
        case Format::JSON:
          return from_json(bytes);
        case Format::CBOR:
          return from_cbor(bytes);
        case Format::Text:
          return from_text(
              bytes, inputs.text_style.value, inputs.text_type.value,
              inputs.delimiter.value, inputs.line_ending.value);
        case Format::Binary:
          return from_binary(
              bytes, inputs.byte_order.value, inputs.scalar_type.value,
              inputs.binary_mode.value == BinaryMode::Layout
                  ? &cache.get(inputs.layout.value, inputs.byte_order.value)
                  : nullptr);
      }
      throw codec_error{"Unknown format"};
    });
  }
};

struct Serialize
{
  halp_meta(name, "Serialize")
  halp_meta(c_name, "avnd_serialize")
  halp_meta(author, "ossia team")
  halp_meta(category, "Control/Serialization")
  halp_meta(uuid, "5d265cd1-014f-48bb-9c51-d594c8b58b62")
  halp_meta(
      description,
      "Encode Value as strict JSON, JSON-compatible CBOR, UTF-8 text or binary. "
      "Plain Text infers shape: raw strings, delimited flat lists/vectors, nested JSON; "
      "vectors decode as lists. Pretty Text is ossia display, not a lossless archive. "
      "Free-flow binary flattens lists/vectors; maps and impulses fail. "
      "Limits: 8 MiB, 65536 nodes, 64 containers. Errors clear Bytes.")
  struct ins
  {
    struct : halp::val_port<"Value", ossia::value>
    {
      void update(Serialize& self) { self.process(); }
    } input;
    halp::enum_t<value_serialization::Format, "Format"> format;
    struct : halp::toggle<"Pretty print">
    {
      halp_meta(
          description, "Indent JSON without changing its values or numeric precision.")
    } pretty;
    struct : halp::enum_t<value_serialization::TextStyle, "Text style">
    {
      halp_meta(
          description,
          "Plain infers shape: root strings are raw UTF-8, scalars are plain tokens, "
          "flat lists/vectors are delimited JSON tokens, nested containers use JSON. "
          "Pretty uses ossia type labels and two-decimal floats: lossy display; maps "
          "and ambiguous escaped strings cannot be decoded.")
    } text_style;
    struct : halp::lineedit<"Delimiter", ",">
    {
      halp_meta(
          description,
          "Separator for nonempty flat Plain lists and vectors. Strings are "
          "JSON-quoted; numeric delimiter collisions fail. Must exclude quotes, "
          "backslashes and brackets. One-field lists require List interpretation when "
          "decoding.")
    } delimiter;
    struct : halp::enum_t<value_serialization::LineEnding, "Line ending">
    {
      halp_meta(
          description,
          "Append a line ending to Text, except raw Plain strings which are unchanged.")
    } line_ending;
    struct : halp::enum_t<value_serialization::ByteOrder, "Byte order">
    {
      halp_meta(
          description, "Free-flow endian and the default for layouts without a prefix.")
    } byte_order;
    struct : halp::enum_t<value_serialization::BinaryMode, "Binary mode">
    {
      halp_meta(
          description,
          "FreeFlow writes int32, float32, bool bytes and raw strings, flattening lists "
          "and vectors without tags. Layout writes explicitly typed fields.")
    } binary_mode;
    struct : halp::lineedit<"Layout", "<B H I f 4s 2x">
    {
      halp_meta(
          description,
          "Python struct subset: < > ! = packed, @ native endian and alignment; "
          "no prefix uses Byte order and packed widths. [count]b B h H i I q Q f d ? s "
          "x. "
          "Counts <=65536; zero numeric counts align only. Ns consumes one exact-size "
          "raw string, never padded or truncated; Nx writes zeros. No automatic end "
          "padding.")
    } layout;
  } inputs;
  struct
  {
    halp::callback<"Bytes", std::string> result;
    halp::callback<"Success", bool> success;
    halp::callback<"Error", std::string> error;
  } outputs;

  struct ui
  {
    halp_meta(layout, halp::layouts::vbox)
    halp::item<&ins::format> format;
    struct Settings
    {
      halp_meta(layout, halp::layouts::tabs)
      halp_flag(hide_tabs);
      static constexpr auto model = &ins::format;
      struct
      {
        halp_meta(name, "JSON")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::pretty> pretty;
      } json;
      struct
      {
        halp_meta(name, "CBOR")
        halp_meta(layout, halp::layouts::vbox)
        halp::label types{.text = "JSON-compatible CBOR: int32, finite float32."};
        halp::label values{.text = "Impulse = null; vectors/lists = arrays."};
        halp::label keys{.text = "UTF-8 text and unique map keys required."};
        halp::label limits{.text = "8 MiB, 65536 values/keys, 64 containers."};
      } cbor;
      struct
      {
        halp_meta(name, "Text")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::text_style> style;
        halp::item<&ins::delimiter> delimiter;
        halp::item<&ins::line_ending> ending;
      } text;
      struct
      {
        halp_meta(name, "Binary")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::byte_order> order;
        halp::item<&ins::binary_mode> mode;
        halp::item<&ins::layout> format_text;
      } binary;
    } settings;
  };

  value_serialization::layout_cache cache;
  void process()
  {
    using namespace value_serialization;
    publish(outputs, [&]() -> std::string {
      const auto& value = inputs.input.value;
      switch(inputs.format.value)
      {
        case Format::JSON:
          return inputs.pretty.value ? encode<pretty_json_writer>(value)
                                     : encode<json_writer>(value);
        case Format::CBOR:
          return encode<cbor_writer>(value);
        case Format::Text:
          return to_text(
              value, inputs.text_style.value, inputs.delimiter.value,
              inputs.line_ending.value);
        case Format::Binary:
          return to_binary(
              value, inputs.byte_order.value,
              inputs.binary_mode.value == BinaryMode::Layout
                  ? &cache.get(inputs.layout.value, inputs.byte_order.value)
                  : nullptr);
      }
      throw codec_error{"Unknown format"};
    });
  }
};
}
