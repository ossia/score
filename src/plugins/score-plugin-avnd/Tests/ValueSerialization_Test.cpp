#include <AvndProcesses/ValueSerialization.hpp>
#include <catch2/catch_all.hpp>

#include <cmath>

#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{
using namespace avnd_tools::value_serialization;
template <typename Process, typename Result>
struct observed
{
  Process process;
  Result result{};
  bool success{};
  std::string error;
  int events{};

  observed(Format format = Format::JSON)
  {
    process.inputs.format.value = format;
    process.outputs.result.call.context = this;
    process.outputs.result.call.function = [](void* ptr, Result value) {
      auto& self = *static_cast<observed*>(ptr);
      self.result = std::move(value);
      ++self.events;
    };
    process.outputs.success.call.context = this;
    process.outputs.success.call.function
        = [](void* ptr, bool value) { static_cast<observed*>(ptr)->success = value; };
    process.outputs.error.call.context = this;
    process.outputs.error.call.function = [](void* ptr, std::string value) {
      static_cast<observed*>(ptr)->error = std::move(value);
    };
  }

  template <typename Input>
  void send(Input&& value)
  {
    process.inputs.input.value = std::forward<Input>(value);
    process.inputs.input.update(process);
  }
};

std::string bytes(std::initializer_list<unsigned char> data)
{
  return {reinterpret_cast<const char*>(data.begin()), data.size()};
}

ossia::value sample()
{
  return ossia::value_map_type{
      {"items",
       std::vector<ossia::value>{
           ossia::value_map_type{
               {"name", std::string{"caf\xc3\xa9"}},
               {std::string{"a\0b", 3}, std::string{"x\0y", 3}}},
           std::vector<ossia::value>{}, ossia::value_map_type{}, true, false,
           ossia::impulse{}, -12, 1.25f}},
      {"empty", std::string{}}};
}
}

TEST_CASE(
    "JSON maps and nested arrays round trip through update ports",
    "[value_serialization]")
{
  observed<avnd_tools::Serialize, std::string> encoder;
  observed<avnd_tools::Deserialize, ossia::value> decoder;
  const auto source = sample();
  encoder.send(source);
  REQUIRE(encoder.success);
  REQUIRE(encoder.error.empty());
  decoder.send(encoder.result);
  REQUIRE(decoder.success);
  REQUIRE(decoder.result == source);
  REQUIRE(decoder.result.target<ossia::value_map_type>());

  // Equal incoming events remain events, without polling / DSP tick parsing.
  decoder.send(encoder.result);
  REQUIRE(decoder.events == 2);
}

TEST_CASE(
    "CBOR maps and nested arrays round trip through update ports",
    "[value_serialization]")
{
  observed<avnd_tools::Serialize, std::string> encoder;
  observed<avnd_tools::Deserialize, ossia::value> decoder;
  encoder.process.inputs.format.value = Format::CBOR;
  decoder.process.inputs.format.value = Format::CBOR;
  const auto source = sample();
  encoder.send(source);
  REQUIRE(encoder.success);
  decoder.send(encoder.result);
  REQUIRE(decoder.success);
  REQUIRE(decoder.result == source);

  encoder.send(0);
  REQUIRE(encoder.result == bytes({0x00}));
  decoder.send(encoder.result);
  REQUIRE(decoder.success);
  REQUIRE(decoder.result.get<int>() == 0);
}

TEST_CASE(
    "Codecs preserve supported scalars and document vector loss",
    "[value_serialization]")
{
  const auto format = GENERATE(Format::JSON, Format::CBOR);
  observed<avnd_tools::Serialize, std::string> encoder;
  observed<avnd_tools::Deserialize, ossia::value> decoder;
  encoder.process.inputs.format.value = format;
  decoder.process.inputs.format.value = format;
  const std::vector<ossia::value> values{
      std::numeric_limits<int>::min(),
      std::numeric_limits<int>::max(),
      std::numeric_limits<float>::max(),
      std::numeric_limits<float>::denorm_min(),
      -0.f,
      true,
      false,
      ossia::impulse{},
      std::string{},
      std::string{"a\0b", 3},
      std::string{"\xf0\x9f\x8e\xb5"}};
  for(const auto& value : values)
  {
    encoder.send(value);
    REQUIRE(encoder.success);
    decoder.send(encoder.result);
    REQUIRE(decoder.success);
    REQUIRE(decoder.result == value);
    REQUIRE(decoder.result.get_type() == value.get_type());
  }
  encoder.send(-0.f);
  decoder.send(encoder.result);
  REQUIRE(std::signbit(decoder.result.template get<float>()));

  encoder.send(ossia::vec2f{1.f, 2.f});
  decoder.send(encoder.result);
  REQUIRE(decoder.success);
  REQUIRE(decoder.result == ossia::value{std::vector<ossia::value>{1.f, 2.f}});
  REQUIRE(decoder.result.template target<std::vector<ossia::value>>());
  encoder.send(ossia::vec3f{1.f, 2.f, 3.f});
  decoder.send(encoder.result);
  REQUIRE(decoder.result == ossia::value{std::vector<ossia::value>{1.f, 2.f, 3.f}});
  encoder.send(ossia::vec4f{1.f, 2.f, 3.f, 4.f});
  decoder.send(encoder.result);
  REQUIRE(decoder.result == ossia::value{std::vector<ossia::value>{1.f, 2.f, 3.f, 4.f}});
}

TEST_CASE(
    "JSON failure replaces successful data and later updates recover",
    "[value_serialization]")
{
  observed<avnd_tools::Deserialize, ossia::value> decoder;
  const std::vector<std::string> invalid{
      "",
      "{",
      "[1,]",
      "/*comment*/1",
      "null false",
      "[NaN]",
      "Infinity",
      "2147483648",
      "-2147483649",
      "18446744073709551616",
      "1e39",
      "1e-50",
      "1e-999",
      "{\"a\":1,\"a\":2}",
      "\"\\ud800\"",
      "\"\xc0\xaf\"",
      std::string{"true\0false", 10},
      "\"unescaped\nnewline\""};
  for(const auto& input : invalid)
  {
    CAPTURE(input);
    decoder.send("42");
    REQUIRE(decoder.success);
    decoder.send(input);
    REQUIRE_FALSE(decoder.success);
    REQUIRE_FALSE(decoder.error.empty());
    REQUIRE_FALSE(decoder.result.valid());
  }
  decoder.send(" \n [1, 0.1, null] \t ");
  REQUIRE(decoder.success);
  REQUIRE(decoder.error.empty());
  const auto& list = decoder.result.get<std::vector<ossia::value>>();
  REQUIRE(list[0].target<int>());
  REQUIRE(list[1].get<float>() == static_cast<float>(0.1));
  REQUIRE(list[2].target<ossia::impulse>());
}

TEST_CASE(
    "CBOR rejects unsupported, malformed and trailing input", "[value_serialization]")
{
  observed<avnd_tools::Deserialize, ossia::value> decoder;
  decoder.process.inputs.format.value = Format::CBOR;
  const std::vector<std::string> invalid{
      {},
      bytes({0x18}),
      bytes({0x81}),
      bytes({0xff}),
      bytes({0x00, 0x01}),
      bytes({0xc0, 0x00}),
      bytes({0xf7}),
      bytes({0x41, 0x00}),
      bytes({0xa1, 0x01, 0x02}),
      bytes({0xa1, 0x61, 'a'}),
      bytes({0xa2, 0x61, 'a', 0x01, 0x61, 'a', 0x02}),
      bytes({0x1a, 0x80, 0, 0, 0}),
      bytes({0x3a, 0x80, 0, 0, 0}),
      bytes({0x3b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}),
      bytes({0xf9, 0x7c, 0x00}),
      bytes({0xf9, 0x7e, 0x00}),
      bytes({0xfb, 0x7f, 0xef, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}),
      bytes({0xfb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}),
      bytes({0x62, 0xc0, 0xaf}),
      bytes({0x63, 'x'}),
      bytes({0x7f, 0x61, 0xc3, 0x61, 0xa9, 0xff}),
      bytes({0x7b, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff})};
  for(const auto& input : invalid)
  {
    decoder.send(bytes({0x01}));
    REQUIRE(decoder.success);
    decoder.send(input);
    REQUIRE_FALSE(decoder.success);
    REQUIRE_FALSE(decoder.error.empty());
    REQUIRE_FALSE(decoder.result.valid());
  }

  // Indefinite containers and chunked text are valid CBOR, not trailing data.
  decoder.send(bytes({0x9f, 0x01, 0x7f, 0x61, 'a', 0x61, 'b', 0xff, 0xff}));
  REQUIRE(decoder.success);
  REQUIRE(decoder.error.empty());
  REQUIRE(
      decoder.result == ossia::value{std::vector<ossia::value>{1, std::string{"ab"}}});
  decoder.send(bytes({0xf9, 0x3e, 0x00}));
  REQUIRE(decoder.success);
  REQUIRE(decoder.result.get<float>() == 1.5f);
}

TEST_CASE("Encoders reject unsafe values without stale output", "[value_serialization]")
{
  observed<avnd_tools::Serialize, std::string> encoder;
  encoder.process.inputs.format.value = GENERATE(Format::JSON, Format::CBOR);
  const std::vector<ossia::value> invalid{
      ossia::value{},
      std::numeric_limits<float>::infinity(),
      std::numeric_limits<float>::quiet_NaN(),
      std::string{"\xed\xa0\x80"},
      ossia::vec3f{0.f, std::numeric_limits<float>::infinity(), 0.f},
      std::vector<ossia::value>{1, ossia::value{}},
      ossia::value_map_type{{"x", 1}, {"x", 2}},
      ossia::value_map_type{{std::string{"\xff"}, 1}}};
  for(const auto& input : invalid)
  {
    encoder.send(42);
    REQUIRE(encoder.success);
    encoder.send(input);
    REQUIRE_FALSE(encoder.success);
    REQUIRE_FALSE(encoder.error.empty());
    REQUIRE(encoder.result.empty());
  }
  encoder.send(ossia::impulse{});
  REQUIRE(encoder.success);
  REQUIRE(encoder.error.empty());
}

TEST_CASE(
    "Encoder resource limits are enforced during traversal", "[value_serialization]")
{
  observed<avnd_tools::Serialize, std::string> encoder;
  encoder.process.inputs.format.value = GENERATE(Format::JSON, Format::CBOR);
  ossia::value nested = 0;
  for(std::size_t i = 0; i < avnd_tools::value_serialization::max_depth; ++i)
    nested = std::vector<ossia::value>{std::move(nested)};
  encoder.send(nested);
  REQUIRE(encoder.success);
  encoder.send(std::vector<ossia::value>{std::move(nested)});
  REQUIRE_FALSE(encoder.success);
  REQUIRE(encoder.result.empty());

  encoder.send(std::vector<ossia::value>(avnd_tools::value_serialization::max_nodes, 0));
  REQUIRE_FALSE(encoder.success);
  REQUIRE(encoder.result.empty());
  encoder.send(std::string(avnd_tools::value_serialization::max_bytes, '\0'));
  REQUIRE_FALSE(encoder.success);
  REQUIRE(encoder.result.empty());
}

TEST_CASE(
    "Decoders bound input bytes, depth and element counts", "[value_serialization]")
{
  observed<avnd_tools::Deserialize, ossia::value> json;
  observed<avnd_tools::Deserialize, ossia::value> cbor;
  cbor.process.inputs.format.value = Format::CBOR;
  const auto depth = avnd_tools::value_serialization::max_depth;
  json.send(std::string(depth, '[') + "0" + std::string(depth, ']'));
  REQUIRE(json.success);
  json.send(std::string(depth + 1, '[') + "0" + std::string(depth + 1, ']'));
  REQUIRE_FALSE(json.success);
  cbor.send(std::string(depth, static_cast<char>(0x81)) + bytes({0x00}));
  REQUIRE(cbor.success);
  cbor.send(std::string(depth + 1, static_cast<char>(0x81)) + bytes({0x00}));
  REQUIRE_FALSE(cbor.success);

  std::string many{"[0"};
  for(std::size_t i = 1; i < avnd_tools::value_serialization::max_nodes; ++i)
    many += ",0";
  many += ']';
  json.send(many);
  REQUIRE_FALSE(json.success);
  cbor.send(
      bytes({0x9a, 0x00, 0x01, 0x00, 0x00})
      + std::string(avnd_tools::value_serialization::max_nodes, '\0'));
  REQUIRE_FALSE(cbor.success);

  const std::string huge(avnd_tools::value_serialization::max_bytes + 1, ' ');
  json.send(huge);
  cbor.send(huge);
  REQUIRE_FALSE(json.success);
  REQUIRE_FALSE(cbor.success);
  REQUIRE_FALSE(json.result.valid());
  REQUIRE_FALSE(cbor.result.valid());
}

TEST_CASE("JSON pretty output remains strict and bounded", "[value_serialization]")
{
  observed<avnd_tools::Serialize, std::string> encoder;
  observed<avnd_tools::Deserialize, ossia::value> decoder;
  encoder.process.inputs.pretty.value = true;
  encoder.send(sample());
  REQUIRE(encoder.success);
  REQUIRE(encoder.result.find('\n') != std::string::npos);
  decoder.send(encoder.result);
  REQUIRE(decoder.success);
  REQUIRE(decoder.result == sample());
  encoder.send(std::string(max_bytes, '\0'));
  REQUIRE_FALSE(encoder.success);
  REQUIRE(encoder.result.empty());
}

TEST_CASE(
    "Plain text infers shape and resolves string ambiguity explicitly",
    "[value_serialization]")
{
  observed<avnd_tools::Serialize, std::string> encoder{Format::Text};
  observed<avnd_tools::Deserialize, ossia::value> decoder{Format::Text};
  decoder.process.inputs.text_type.value = TextType::String;
  encoder.process.inputs.line_ending.value = LineEnding::CRLF;
  decoder.process.inputs.line_ending.value = LineEnding::CRLF;
  for(const auto& text :
      {std::string{"caf\xc3\xa9\0\n", 7}, std::string{"[1,true]"}, std::string{"123"},
       std::string{"true"}, std::string{"a,b"}, std::string{}})
  {
    encoder.send(text);
    REQUIRE(encoder.success);
    REQUIRE(encoder.result == text);
    decoder.send(encoder.result);
    REQUIRE(decoder.success);
    REQUIRE(decoder.result == ossia::value{text});
  }
  decoder.process.inputs.text_type.value = TextType::Auto;
  encoder.send(sample());
  REQUIRE(encoder.success);
  decoder.send(encoder.result);
  REQUIRE(decoder.success);
  REQUIRE(decoder.result == sample());
  encoder.send(std::vector<ossia::value>{});
  decoder.send(encoder.result);
  REQUIRE(decoder.success);
  REQUIRE(decoder.result == ossia::value{std::vector<ossia::value>{}});
  decoder.send(std::string{"\xff"});
  REQUIRE_FALSE(decoder.success);
  REQUIRE_FALSE(decoder.result.valid());
}

TEST_CASE(
    "Text scalar interpretation rejects overflow without losing float precision",
    "[value_serialization]")
{
  observed<avnd_tools::Serialize, std::string> encoder{Format::Text};
  observed<avnd_tools::Deserialize, ossia::value> decoder{Format::Text};
  for(const float value :
      {std::numeric_limits<float>::max(), std::numeric_limits<float>::denorm_min(),
       2147483648.f, -0.f, 0.123456789f})
  {
    encoder.send(value);
    REQUIRE(encoder.success);
    decoder.send(encoder.result);
    REQUIRE(decoder.success);
    REQUIRE(decoder.result.target<float>());
    REQUIRE(decoder.result.get<float>() == value);
    REQUIRE(std::signbit(decoder.result.get<float>()) == std::signbit(value));
  }
  for(const auto* text :
      {"2147483648", "-2147483649", "1e39", "1e-50", "nan", "inf", "12x"})
  {
    decoder.send(text);
    REQUIRE_FALSE(decoder.success);
    REQUIRE_FALSE(decoder.result.valid());
  }
  decoder.process.inputs.text_type.value = TextType::Float;
  decoder.send("-0");
  REQUIRE(decoder.success);
  REQUIRE(std::signbit(decoder.result.get<float>()));
  decoder.process.inputs.text_type.value = TextType::Boolean;
  decoder.send("1");
  REQUIRE_FALSE(decoder.success);
  decoder.send("false");
  REQUIRE(decoder.success);
  REQUIRE(decoder.result == ossia::value{false});
  decoder.process.inputs.text_type.value = TextType::Integer;
  decoder.send("1.5");
  REQUIRE_FALSE(decoder.success);
  decoder.send("-2147483648");
  REQUIRE(decoder.success);
  REQUIRE(decoder.result == ossia::value{std::numeric_limits<int>::min()});
}

TEST_CASE(
    "Text visits every float vector and nested vectors without list-only dispatch",
    "[value_serialization]")
{
  observed<avnd_tools::Serialize, std::string> encoder{Format::Text};
  observed<avnd_tools::Deserialize, ossia::value> decoder{Format::Text};
  encoder.process.inputs.delimiter.value = "::";
  decoder.process.inputs.delimiter.value = "::";
  const std::vector<std::pair<ossia::value, ossia::value>> values{
      {ossia::vec2f{1.f, -0.f}, std::vector<ossia::value>{1.f, -0.f}},
      {ossia::vec3f{0.123456789f, 2.f, 3.f},
       std::vector<ossia::value>{0.123456789f, 2.f, 3.f}},
      {ossia::vec4f{1.f, 2.f, 3.f, 4.f}, std::vector<ossia::value>{1.f, 2.f, 3.f, 4.f}},
      {ossia::value_map_type{{"v", ossia::vec2f{1.f, 2.f}}},
       ossia::value_map_type{{"v", std::vector<ossia::value>{1.f, 2.f}}}}};
  for(const auto& [source, expected] : values)
  {
    encoder.send(source);
    REQUIRE(encoder.success);
    decoder.send(encoder.result);
    REQUIRE(decoder.success);
    REQUIRE(decoder.result == expected);
  }
}

TEST_CASE(
    "Delimited text escapes strings and makes single-field list interpretation explicit",
    "[value_serialization]")
{
  observed<avnd_tools::Serialize, std::string> encoder{Format::Text};
  observed<avnd_tools::Deserialize, ossia::value> decoder{Format::Text};
  encoder.process.inputs.delimiter.value = "::";
  decoder.process.inputs.delimiter.value = "::";
  encoder.process.inputs.line_ending.value = LineEnding::CRLF;
  decoder.process.inputs.line_ending.value = LineEnding::CRLF;
  const ossia::value source = std::vector<ossia::value>{
      1,     true,   std::string{}, 1.25f, "a::b",
      "123", "true", "a\"",         "b\\", std::string{"x\0y", 3}};
  encoder.send(source);
  REQUIRE(encoder.success);
  decoder.send(encoder.result);
  REQUIRE(decoder.success);
  REQUIRE(decoder.result == source);
  encoder.send(std::vector<ossia::value>{"123"});
  decoder.send(encoder.result);
  REQUIRE(decoder.success);
  REQUIRE(decoder.result == ossia::value{"123"});
  decoder.process.inputs.text_type.value = TextType::List;
  decoder.send(encoder.result);
  REQUIRE(decoder.success);
  REQUIRE(decoder.result == ossia::value{std::vector<ossia::value>{"123"}});
  encoder.process.inputs.delimiter.value = ".";
  encoder.send(ossia::vec2f{1.5f, 2.f});
  REQUIRE_FALSE(encoder.success);
  encoder.process.inputs.delimiter.value.clear();
  encoder.send(ossia::vec2f{1.f, 2.f});
  REQUIRE_FALSE(encoder.success);
  decoder.process.inputs.delimiter.value.clear();
  decoder.send("1");
  REQUIRE_FALSE(decoder.success);
}

TEST_CASE(
    "Pretty text preserves supported labels but deliberately rounds display precision",
    "[value_serialization]")
{
  observed<avnd_tools::Serialize, std::string> encoder{Format::Text};
  observed<avnd_tools::Deserialize, ossia::value> decoder{Format::Text};
  encoder.process.inputs.text_style.value = TextStyle::Pretty;
  decoder.process.inputs.text_style.value = TextStyle::Pretty;
  const ossia::value source = std::vector<ossia::value>{
      12,
      true,
      "hello",
      ossia::vec2f{1.f, 2.f},
      ossia::vec3f{1.f, 2.f, 3.f},
      ossia::vec4f{1.f, 2.f, 3.f, 4.f},
      ossia::impulse{}};
  encoder.send(source);
  REQUIRE(encoder.success);
  REQUIRE(encoder.result == ossia::value_to_pretty_string(source));
  decoder.send(encoder.result);
  REQUIRE(decoder.success);
  REQUIRE(decoder.result == source);
  encoder.send(0.123456789f);
  REQUIRE(encoder.success);
  decoder.send(encoder.result);
  REQUIRE(decoder.success);
  REQUIRE(decoder.result.get<float>() == 0.12f);
  REQUIRE(decoder.result.get<float>() != 0.123456789f);
  encoder.send(ossia::value_map_type{{"value", 42}});
  REQUIRE(encoder.success);
  decoder.send(encoder.result);
  REQUIRE_FALSE(decoder.success);
  for(const auto* text : {"int: 1 junk", "float: 1e-999", "float: inf", "list: [int: 1"})
  {
    decoder.send(text);
    REQUIRE_FALSE(decoder.success);
    REQUIRE_FALSE(decoder.result.valid());
  }
  std::string nested;
  for(std::size_t i = 0; i <= max_depth; ++i)
    nested += "list: [";
  nested += "int: 1";
  nested.append(max_depth + 1, ']');
  decoder.send(nested);
  REQUIRE_FALSE(decoder.success);
}

TEST_CASE(
    "Free-flow binary writes typed widths and raw bytes without metadata",
    "[value_serialization]")
{
  observed<avnd_tools::Serialize, std::string> encoder{Format::Binary};
  encoder.send(
      std::vector<ossia::value>{
          0x12345678, std::vector<ossia::value>{1.f, true}, std::string{"\0\xff", 2}});
  REQUIRE(encoder.success);
  REQUIRE(
      encoder.result == bytes({0x78, 0x56, 0x34, 0x12, 0, 0, 0x80, 0x3f, 1, 0, 0xff}));
  encoder.process.inputs.byte_order.value = ByteOrder::Big;
  encoder.send(ossia::vec2f{1.f, -2.f});
  REQUIRE(encoder.success);
  REQUIRE(encoder.result == bytes({0x3f, 0x80, 0, 0, 0xc0, 0, 0, 0}));
  for(const auto& value : std::vector<ossia::value>{
          ossia::value{}, ossia::impulse{}, ossia::value_map_type{{"x", 1}},
          std::numeric_limits<float>::quiet_NaN()})
  {
    encoder.send(value);
    REQUIRE_FALSE(encoder.success);
    REQUIRE(encoder.result.empty());
  }
  encoder.send(-1);
  REQUIRE(encoder.success);
  REQUIRE(encoder.result == bytes({0xff, 0xff, 0xff, 0xff}));
}

TEST_CASE(
    "Raw binary decoding uses every selected standard scalar width",
    "[value_serialization]")
{
  struct fixture
  {
    ScalarType type;
    std::string wire;
    ossia::value expected;
  };
  const std::vector<fixture> cases{
      {ScalarType::u8, bytes({0xff}), 255},
      {ScalarType::i8, bytes({0x80}), -128},
      {ScalarType::u16, bytes({0xfe, 0xdc}), 65244},
      {ScalarType::i16, bytes({0x80, 0x00}), -32768},
      {ScalarType::u32, bytes({0x12, 0x34, 0x56, 0x78}), 0x12345678},
      {ScalarType::i32, bytes({0x80, 0, 0, 0}), std::numeric_limits<int>::min()},
      {ScalarType::f32, bytes({0x3f, 0xc0, 0, 0}), 1.5f},
      {ScalarType::f64, bytes({0x3f, 0xf8, 0, 0, 0, 0, 0, 0}), 1.5f}};
  observed<avnd_tools::Deserialize, ossia::value> decoder{Format::Binary};
  for(const auto& item : cases)
  {
    decoder.process.inputs.scalar_type.value = item.type;
    decoder.process.inputs.byte_order.value = ByteOrder::Big;
    decoder.send(item.wire);
    REQUIRE(decoder.success);
    REQUIRE(decoder.result == ossia::value{std::vector<ossia::value>{item.expected}});
    decoder.process.inputs.byte_order.value = ByteOrder::Little;
    decoder.send(std::string{item.wire.rbegin(), item.wire.rend()});
    REQUIRE(decoder.success);
    REQUIRE(decoder.result == ossia::value{std::vector<ossia::value>{item.expected}});
  }
  decoder.process.inputs.scalar_type.value = ScalarType::u32;
  decoder.send(bytes({0xff, 0xff, 0xff, 0xff}));
  REQUIRE_FALSE(decoder.success);
  decoder.send(bytes({1, 0, 0}));
  REQUIRE_FALSE(decoder.success);
  decoder.send(bytes({1, 0, 0, 0, 2, 0, 0, 0}));
  REQUIRE(decoder.success);
  REQUIRE(decoder.result == ossia::value{std::vector<ossia::value>{1, 2}});
}

TEST_CASE(
    "Binary floats obey finite float32 range and signed zero policy",
    "[value_serialization]")
{
  observed<avnd_tools::Deserialize, ossia::value> decoder{Format::Binary};
  decoder.process.inputs.byte_order.value = ByteOrder::Big;
  decoder.process.inputs.scalar_type.value = ScalarType::f32;
  for(const auto& wire : {bytes({0x7f, 0x80, 0, 0}), bytes({0x7f, 0xc0, 0, 0})})
  {
    decoder.send(wire);
    REQUIRE_FALSE(decoder.success);
    REQUIRE_FALSE(decoder.result.valid());
  }
  decoder.process.inputs.scalar_type.value = ScalarType::f64;
  for(const auto& wire :
      {bytes({0x7f, 0xf0, 0, 0, 0, 0, 0, 0}), bytes({0x7f, 0xf8, 0, 0, 0, 0, 0, 0}),
       bytes({0x7f, 0xef, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}),
       bytes({0, 0, 0, 0, 0, 0, 0, 1})})
  {
    decoder.send(wire);
    REQUIRE_FALSE(decoder.success);
  }
  decoder.send(bytes({0x80, 0, 0, 0, 0, 0, 0, 0}));
  REQUIRE(decoder.success);
  REQUIRE(std::signbit(decoder.result.get<std::vector<ossia::value>>()[0].get<float>()));
}

TEST_CASE(
    "Struct layouts preserve ordered fields with explicit endian and padding",
    "[value_serialization]")
{
  observed<avnd_tools::Serialize, std::string> encoder{Format::Binary};
  observed<avnd_tools::Deserialize, ossia::value> decoder{Format::Binary};
  encoder.process.inputs.binary_mode.value = BinaryMode::Layout;
  decoder.process.inputs.binary_mode.value = BinaryMode::Layout;
  encoder.process.inputs.layout.value = "<B H I f 4s 2x";
  decoder.process.inputs.layout.value = "<B H I f 4s 2x";
  encoder.process.inputs.byte_order.value = ByteOrder::Big;
  decoder.process.inputs.byte_order.value = ByteOrder::Big;
  const ossia::value fields = std::vector<ossia::value>{
      255, 0x1234, 0x12345678, 1.5f, std::string{"a\0\xffz", 4}};
  encoder.send(fields);
  REQUIRE(encoder.success);
  REQUIRE(
      encoder.result
      == bytes(
          {0xff, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12, 0, 0, 0xc0, 0x3f, 'a', 0, 0xff, 'z',
           0, 0}));
  decoder.send(encoder.result);
  REQUIRE(decoder.success);
  REQUIRE(decoder.result == fields);
  auto padded = encoder.result;
  padded.back() = '\x7f';
  decoder.send(padded);
  REQUIRE(decoder.success);
  REQUIRE(decoder.result == fields);
  decoder.send(encoder.result + "x");
  REQUIRE_FALSE(decoder.success);
  decoder.send(encoder.result.substr(1));
  REQUIRE_FALSE(decoder.success);
  decoder.send(encoder.result);
  REQUIRE(decoder.success);
}

TEST_CASE("Layout repeats and signed widths use no native ABI", "[value_serialization]")
{
  observed<avnd_tools::Serialize, std::string> encoder{Format::Binary};
  observed<avnd_tools::Deserialize, ossia::value> decoder{Format::Binary};
  encoder.process.inputs.binary_mode.value = BinaryMode::Layout;
  decoder.process.inputs.binary_mode.value = BinaryMode::Layout;
  encoder.process.inputs.layout.value = ">2b h i q Q d ?";
  decoder.process.inputs.layout.value = ">2b h i q Q d ?";
  const ossia::value fields = std::vector<ossia::value>{
      -128, 127, -32768, std::numeric_limits<int>::min(), -1, 42, 1.5f, true};
  encoder.send(fields);
  REQUIRE(encoder.success);
  REQUIRE(
      encoder.result == bytes({0x80, 0x7f, 0x80, 0,    0x80, 0, 0, 0, 0xff, 0xff, 0xff,
                               0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0,    0,    0,
                               0,    42,   0x3f, 0xf8, 0,    0, 0, 0, 0,    0,    1}));
  decoder.send(encoder.result);
  REQUIRE(decoder.success);
  REQUIRE(decoder.result == fields);
  decoder.process.inputs.layout.value = ">Q";
  decoder.send(bytes({0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}));
  REQUIRE_FALSE(decoder.success);
  decoder.process.inputs.layout.value = ">q";
  decoder.send(bytes({0x80, 0, 0, 0, 0, 0, 0, 0}));
  REQUIRE_FALSE(decoder.success);
  decoder.process.inputs.layout.value = "?";
  decoder.send(bytes({2}));
  REQUIRE_FALSE(decoder.success);
}

TEST_CASE(
    "Layout errors clear output and edited formats recover", "[value_serialization]")
{
  observed<avnd_tools::Serialize, std::string> encoder{Format::Binary};
  observed<avnd_tools::Deserialize, ossia::value> decoder{Format::Binary};
  encoder.process.inputs.binary_mode.value = BinaryMode::Layout;
  decoder.process.inputs.binary_mode.value = BinaryMode::Layout;
  for(const auto* format :
      {"", "<", "0s", "0x", "65537s", "999999999999B", "4", "2 B", "j", "B>", "65536B"})
  {
    CAPTURE(format);
    encoder.process.inputs.layout.value = format;
    decoder.process.inputs.layout.value = format;
    encoder.send(1);
    decoder.send(bytes({1}));
    REQUIRE_FALSE(encoder.success);
    REQUIRE(encoder.result.empty());
    REQUIRE_FALSE(decoder.success);
    REQUIRE_FALSE(decoder.result.valid());
    // Repeated malformed format uses the cached failure, not a stale valid layout.
    encoder.send(1);
    REQUIRE_FALSE(encoder.success);
  }
  encoder.process.inputs.layout.value = "B";
  for(const auto& value :
      std::vector<ossia::value>{-1, 256, 1.5f, std::vector<ossia::value>{1, 2}})
  {
    encoder.send(value);
    REQUIRE_FALSE(encoder.success);
  }
  encoder.process.inputs.layout.value = "b";
  encoder.send(128);
  REQUIRE_FALSE(encoder.success);
  encoder.process.inputs.layout.value = "H";
  encoder.send(65536);
  REQUIRE_FALSE(encoder.success);
  encoder.process.inputs.layout.value = "?";
  encoder.send(2);
  REQUIRE_FALSE(encoder.success);
  encoder.process.inputs.layout.value = "f";
  encoder.send(std::numeric_limits<float>::quiet_NaN());
  REQUIRE_FALSE(encoder.success);
  encoder.process.inputs.layout.value = "2x";
  encoder.send(std::vector<ossia::value>{});
  REQUIRE(encoder.success);
  REQUIRE(encoder.result == bytes({0, 0}));
  encoder.process.inputs.layout.value = "4s";
  encoder.send(std::string{"abc"});
  REQUIRE_FALSE(encoder.success);
  encoder.send(std::string{"abcde"});
  REQUIRE_FALSE(encoder.success);
  encoder.process.inputs.layout.value = "H";
  decoder.process.inputs.layout.value = "H";
  encoder.send(0x1234);
  REQUIRE(encoder.success);
  REQUIRE(encoder.result == bytes({0x34, 0x12}));
  encoder.process.inputs.byte_order.value = ByteOrder::Big;
  decoder.process.inputs.byte_order.value = ByteOrder::Big;
  encoder.send(0x1234);
  REQUIRE(encoder.result == bytes({0x12, 0x34}));
  decoder.send(encoder.result);
  REQUIRE(decoder.success);
  REQUIRE(decoder.result == ossia::value{std::vector<ossia::value>{0x1234}});
}

TEST_CASE(
    "Native struct alignment and zero repeats match field offsets without trailing "
    "padding",
    "[value_serialization]")
{
  observed<avnd_tools::Serialize, std::string> encoder{Format::Binary};
  observed<avnd_tools::Deserialize, ossia::value> decoder{Format::Binary};
  encoder.process.inputs.binary_mode.value = BinaryMode::Layout;
  decoder.process.inputs.binary_mode.value = BinaryMode::Layout;
  encoder.process.inputs.layout.value = "@B I B 0I";
  decoder.process.inputs.layout.value = "@B I B 0I";
  const ossia::value fields = std::vector<ossia::value>{1, 0x12345678, 2};
  encoder.send(fields);
  REQUIRE(encoder.success);
  const auto alignment = alignof(int);
  const auto integer_offset = (1 + alignment - 1) / alignment * alignment;
  const auto end = integer_offset + sizeof(int) + 1;
  const auto aligned_end = (end + alignment - 1) / alignment * alignment;
  REQUIRE(encoder.result.size() == aligned_end);
  REQUIRE(encoder.result[0] == 1);
  REQUIRE(encoder.result[integer_offset + sizeof(int)] == 2);
  auto native = encoder.result;
  // Native and explicit padding is ignored on decode, not required to be zero.
  for(std::size_t i = 1; i < integer_offset; ++i)
    native[i] = '\x7f';
  decoder.send(native);
  REQUIRE(decoder.success);
  REQUIRE(decoder.result == fields);
  encoder.process.inputs.layout.value = "@B I B";
  encoder.send(fields);
  REQUIRE(encoder.success);
  REQUIRE(encoder.result.size() == end);
  encoder.process.inputs.layout.value = "=B I B 0I";
  decoder.process.inputs.layout.value = "=B I B 0I";
  encoder.send(fields);
  REQUIRE(encoder.success);
  REQUIRE(encoder.result.size() == 6);
  decoder.send(encoder.result);
  REQUIRE(decoder.success);
  REQUIRE(decoder.result == fields);
  encoder.process.inputs.layout.value = "!I";
  encoder.send(0x12345678);
  REQUIRE(encoder.success);
  REQUIRE(encoder.result == bytes({0x12, 0x34, 0x56, 0x78}));
  encoder.process.inputs.layout.value = "<2f";
  decoder.process.inputs.layout.value = "<2f";
  encoder.send(ossia::vec2f{1.5f, -2.25f});
  REQUIRE(encoder.success);
  decoder.send(encoder.result);
  REQUIRE(decoder.success);
  REQUIRE(decoder.result == ossia::value{std::vector<ossia::value>{1.5f, -2.25f}});
}

TEST_CASE(
    "Text and binary enforce byte node depth and layout count budgets",
    "[value_serialization]")
{
  observed<avnd_tools::Serialize, std::string> encoder;
  observed<avnd_tools::Deserialize, ossia::value> decoder;
  const auto format = GENERATE(Format::Text, Format::Binary);
  encoder.process.inputs.format.value = format;
  decoder.process.inputs.format.value = format;
  const std::string huge(max_bytes + 1, 'a');
  encoder.send(huge);
  decoder.send(huge);
  REQUIRE_FALSE(encoder.success);
  REQUIRE_FALSE(decoder.success);
  encoder.process.inputs.format.value = Format::Binary;
  ossia::value nested = 0;
  for(std::size_t i = 0; i < max_depth; ++i)
    nested = std::vector<ossia::value>{std::move(nested)};
  encoder.send(nested);
  REQUIRE(encoder.success);
  encoder.send(std::vector<ossia::value>{std::move(nested)});
  REQUIRE_FALSE(encoder.success);
  encoder.send(std::vector<ossia::value>(max_nodes, 0));
  REQUIRE_FALSE(encoder.success);
  decoder.process.inputs.format.value = Format::Binary;
  decoder.send(std::string(max_nodes, '\0'));
  REQUIRE_FALSE(decoder.success);
  decoder.process.inputs.format.value = Format::Text;
  decoder.process.inputs.text_type.value = TextType::List;
  decoder.send(std::string(max_nodes, ','));
  REQUIRE_FALSE(decoder.success);
  encoder.process.inputs.binary_mode.value = BinaryMode::Layout;
  encoder.process.inputs.layout.value = "65536s";
  encoder.send(std::string(65536, '\0'));
  REQUIRE(encoder.success);
  encoder.process.inputs.layout.value = std::string(max_nodes + 1, 'x');
  encoder.send(std::vector<ossia::value>{});
  REQUIRE_FALSE(encoder.success);
  encoder.process.inputs.layout.value.clear();
  for(int i = 0; i < 129; ++i)
    encoder.process.inputs.layout.value += "65536s ";
  encoder.send(std::vector<ossia::value>{});
  REQUIRE_FALSE(encoder.success);
}

TEST_CASE(
    "Format switches apply on the next input and isolate layout failures",
    "[value_serialization]")
{
  observed<avnd_tools::Serialize, std::string> encoder;
  observed<avnd_tools::Deserialize, ossia::value> decoder;
  encoder.send(1);
  REQUIRE(encoder.result == "1");
  encoder.process.inputs.format.value = Format::CBOR;
  decoder.process.inputs.format.value = Format::CBOR;
  encoder.send(1);
  REQUIRE(encoder.result == bytes({1}));
  decoder.send(encoder.result);
  REQUIRE(decoder.result == ossia::value{1});
  encoder.process.inputs.format.value = Format::Text;
  decoder.process.inputs.format.value = Format::Text;
  encoder.send(1);
  REQUIRE(encoder.result == "1");
  decoder.send(encoder.result);
  REQUIRE(decoder.result == ossia::value{1});
  encoder.process.inputs.format.value = Format::Binary;
  decoder.process.inputs.format.value = Format::Binary;
  encoder.process.inputs.binary_mode.value = BinaryMode::Layout;
  decoder.process.inputs.binary_mode.value = BinaryMode::Layout;
  encoder.process.inputs.layout.value = "broken";
  decoder.process.inputs.layout.value = "broken";
  encoder.send(1);
  decoder.send("1");
  REQUIRE_FALSE(encoder.success);
  REQUIRE_FALSE(decoder.success);
  encoder.process.inputs.format.value = Format::JSON;
  decoder.process.inputs.format.value = Format::JSON;
  encoder.send(1);
  decoder.send(encoder.result);
  REQUIRE(encoder.success);
  REQUIRE(encoder.error.empty());
  REQUIRE(decoder.success);
  REQUIRE(decoder.error.empty());
  REQUIRE(decoder.result == ossia::value{1});
}
