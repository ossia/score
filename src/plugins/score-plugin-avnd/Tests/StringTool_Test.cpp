#include <Advanced/Utilities/StringTool.hpp>
#include <AvndProcesses/StringBytes.hpp>
#include <catch2/catch_all.hpp>

#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{
template <typename Object, typename Value = std::string>
struct StringHarness
{
  Object object;
  std::vector<Value> values;
  std::vector<std::string> errors;
  std::string events;

  StringHarness()
  {
    auto& result = [&]() -> auto& {
      if constexpr(requires { object.outputs.text; })
        return object.outputs.text;
      else if constexpr(requires { object.outputs.parts; })
        return object.outputs.parts;
      else
        return object.outputs.bytes;
    }();
    result.call.context = this;
    result.call.function = [](void* context, Value value) {
      auto& self = *static_cast<StringHarness*>(context);
      self.values.push_back(std::move(value));
      self.events += 'v';
    };
    object.outputs.error.call.context = this;
    object.outputs.error.call.function = [](void* context, std::string error) {
      auto& self = *static_cast<StringHarness*>(context);
      self.errors.push_back(std::move(error));
      self.events += 'e';
    };
  }

  template <typename Input>
  void send(const Input& input)
  {
    values.clear();
    errors.clear();
    events.clear();
    if constexpr(requires { typename Object::messages; })
    {
      typename Object::messages::message message;
      message(object, input);
    }
    else
    {
      object.inputs.input.value = input;
      object.inputs.input.update(object);
    }
  }

  void success(const Value& expected)
  {
    REQUIRE(values == std::vector<Value>{expected});
    REQUIRE(errors == std::vector<std::string>{""});
    REQUIRE(events == "ev");
  }

  void failure()
  {
    REQUIRE(values.empty());
    REQUIRE(errors.size() == 1);
    REQUIRE_FALSE(errors.front().empty());
    REQUIRE(events == "e");
  }
};
using Tool = StringHarness<ao::StringTool>;
using Split = StringHarness<ao::StringSplit, std::vector<std::string>>;
using Join = StringHarness<ao::StringJoin>;
using ToBytes = StringHarness<avnd_tools::StringToBytes, std::vector<int>>;
using FromBytes = StringHarness<avnd_tools::BytesToString>;
}

TEST_CASE(
    "String tool preserves text and has a deterministic transformation chain",
    "[avnd][string]")
{
  Tool h;
  h.send("a é東京😀\n");
  h.success("a é東京😀\n");
  h.send("");
  h.success("");
  auto& in = h.object.inputs;
  in.trim = ao::StringTrim::Both;
  in.slice.value = true;
  in.start.value = 1;
  in.length.value = 2;
  in.replace.value = true;
  in.find.value = "é";
  in.replacement.value = "xy";
  in.casing = ao::StringCase::ASCII_Upper;
  in.reverse.value = true;
  in.rotate.value = 1;
  in.repeat.value = 2;
  in.pad_left.value = 1;
  in.pad_right.value = 2;
  in.pad.value = "😀";
  // aéBz -> éB -> xyB -> XYB -> BYX -> YXB -> YXBYXB
  h.send(" \taéBz\r\n");
  h.success("😀YXBYXB😀😀");
}

TEST_CASE("String trim and case are explicitly ASCII only", "[avnd][string]")
{
  Tool h;
  auto& in = h.object.inputs;
  in.trim = ao::StringTrim::Left;
  h.send("\t\v\fa \n");
  h.success("a \n");
  in.trim = ao::StringTrim::Right;
  h.send("\ta \n\r");
  h.success("\ta");
  in.trim = ao::StringTrim::Both;
  h.send(" \t\r\n\f\v");
  h.success("");
  in.casing = ao::StringCase::ASCII_Lower;
  h.send(" AZÉİΣß  ");
  h.success("azÉİΣß "); // nonbreaking space is not ASCII whitespace
  in.casing = ao::StringCase::ASCII_Upper;
  h.send(" azéıσß ");
  h.success("AZéıσß");
}

TEST_CASE(
    "String slicing clamps codepoint indices without splitting UTF-8", "[avnd][string]")
{
  Tool h;
  auto& in = h.object.inputs;
  in.slice.value = true;
  in.start.value = -2;
  in.length.value = 1;
  h.send("aé東😀");
  h.success("東");
  in.start.value = std::numeric_limits<int>::min();
  in.length.value = -1;
  h.send("aé東😀");
  h.success("aé東😀");
  in.start.value = std::numeric_limits<int>::max();
  h.send("aé東😀");
  h.success("");
  in.start.value = 1;
  in.length.value = std::numeric_limits<int>::max();
  h.send("aé東😀");
  h.success("é東😀");
  in.length.value = 0;
  h.send("aé東😀");
  h.success("");
  in.length.value = -2;
  h.send("aé東😀");
  h.failure();
}

TEST_CASE(
    "String literal replacement is nonoverlapping and never recursive", "[avnd][string]")
{
  Tool h;
  auto& in = h.object.inputs;
  in.replace.value = true;
  in.find.value = "aa";
  in.replacement.value = "aaa";
  h.send("aaaaa");
  h.success("aaaaaaa");
  in.find.value = ".*";
  in.replacement.value = "$1\\x";
  h.send("a.*b.*");
  h.success("a$1\\xb$1\\x");
  in.find.value = "東京";
  in.replacement.value = "";
  h.send("a東京é東京");
  h.success("aé");
  h.send("no match");
  h.success("no match");
  in.find.value = "";
  h.send("");
  h.failure();
}

TEST_CASE(
    "Reverse and rotation operate on Unicode codepoints including NUL", "[avnd][string]")
{
  Tool h;
  auto& in = h.object.inputs;
  in.reverse.value = true;
  h.send("aé東京😀");
  h.success("😀京東éa");
  h.send(std::string{"A\0é", 4});
  h.success(std::string{"é\0A", 4});
  h.send("e\xCC\x81");
  h.success(
      "\xCC\x81"
      "e"); // codepoints, not graphemes
  in.reverse.value = false;
  in.rotate.value = -1;
  h.send("aé東😀");
  h.success("😀aé東");
  in.rotate.value = 5;
  h.send("aé東😀");
  h.success("é東😀a");
  in.rotate.value = std::numeric_limits<int>::min();
  h.send("aé東");
  h.success("é東a");
  h.send("");
  h.success("");
}

TEST_CASE(
    "Repeat zero and Unicode padding work on empty and nonempty strings",
    "[avnd][string]")
{
  Tool h;
  auto& in = h.object.inputs;
  in.repeat.value = 0;
  h.send("é");
  h.success("");
  in.pad_left.value = 2;
  in.pad_right.value = 1;
  in.pad.value = "界";
  h.send("discarded");
  h.success("界界界");
  in.repeat.value = std::numeric_limits<int>::max();
  h.send("");
  h.success("界界界");
  in.repeat.value = 3;
  h.send("é");
  h.success("界界ééé界");
  in.repeat.value = 1;
  in.pad_left.value = 0;
  h.send("é東");
  h.success("é東界");
}

TEST_CASE("String length limits apply after existing transformations", "[avnd][string]")
{
  Tool h;
  auto& in = h.object.inputs;
  in.repeat.value = 2;
  in.pad_left.value = 1;
  in.pad.value = "界";
  in.min_length.value = 5;
  in.max_length.value = 5;
  in.min_pad.value = "😀";
  h.send("é");
  h.success("界éé😀😀");
  h.send("é東😀");
  h.success("界é東😀é");
  in.repeat.value = 1;
  in.pad_left.value = 0;
  in.min_length.value = 0;
  in.max_length.value = 2;
  h.send("é東😀");
  h.success("é東");
  h.send(std::string{"é\0😀", 7});
  h.success(std::string{"é\0", 3});
  in.max_length.value = 0;
  h.send("é東😀");
  h.success("");
  in.max_length.value = -1;
  in.min_pad.value = "\xFF"; // Disabled minimum does not inspect unused padding.
  h.send("é東😀");
  h.success("é東😀");
}

TEST_CASE(
    "Minimum length padding preserves codepoint and byte boundaries", "[avnd][string]")
{
  Tool h;
  auto& in = h.object.inputs;
  in.min_length.value = 3;
  in.min_pad.value = "😀";
  in.max_bytes.value = 10;
  h.send("é");
  h.success("é😀😀"); // Exactly ten bytes, three codepoints.
  in.max_bytes.value = 9;
  h.send("é");
  h.failure();
  in.min_pad.value = std::string(1, '\0');
  in.max_bytes.value = 4;
  h.send("é");
  h.success(std::string{"é\0\0", 4});
  in.min_pad.value = "東";
  in.max_bytes.value = 9;
  h.send("");
  h.success("東東東");
}

TEST_CASE(
    "String length constraints reject impossible or invalid options atomically",
    "[avnd][string]")
{
  Tool h;
  auto& in = h.object.inputs;
  SECTION("contradictory active lengths")
  {
    in.min_length.value = 3;
    in.max_length.value = 2;
  }
  SECTION("positive minimum and zero maximum")
  {
    in.min_length.value = 1;
    in.max_length.value = 0;
  }
  SECTION("minimum above byte budget")
  {
    in.min_length.value = 3;
    in.max_bytes.value = 2;
  }
  SECTION("invalid negative minimum")
  {
    in.min_length.value = -1;
  }
  SECTION("invalid negative maximum")
  {
    in.max_length.value = -2;
  }
  SECTION("minimum above hard ceiling")
  {
    in.min_length.value = std::numeric_limits<int>::max();
  }
  SECTION("maximum above hard ceiling")
  {
    in.max_length.value = std::numeric_limits<int>::max();
  }
  SECTION("empty minimum pad")
  {
    in.min_length.value = 2;
    in.min_pad.value = "";
  }
  SECTION("multiple codepoints in minimum pad")
  {
    in.min_length.value = 2;
    in.min_pad.value = "e\xCC\x81";
  }
  SECTION("malformed minimum pad")
  {
    in.min_length.value = 2;
    in.min_pad.value = "\xFF";
  }
  SECTION("final truncation does not bypass intermediate byte limit")
  {
    in.max_length.value = 1;
    in.max_bytes.value = 2;
    in.repeat.value = 3;
  }
  h.send("a");
  h.failure();
  in.min_length.value = 1;
  in.max_length.value = 1;
  in.max_bytes.value = 2;
  in.repeat.value = 1;
  in.min_pad.value = " ";
  h.send("é");
  h.success("é");
}

TEST_CASE(
    "String growth limits fail atomically and recover on the next message",
    "[avnd][string]")
{
  Tool h;
  auto& in = h.object.inputs;
  in.max_bytes.value = 6;
  SECTION("input")
  {
    h.send("1234567");
  }
  SECTION("replacement")
  {
    in.replace.value = true;
    in.find.value = "a";
    in.replacement.value = "é";
    h.send("aaaa");
  }
  SECTION("repeat")
  {
    in.repeat.value = 4;
    h.send("é");
  }
  SECTION("padding")
  {
    in.pad_left.value = 2;
    in.pad.value = "😀";
    h.send("");
  }
  SECTION("maximum integer repeat")
  {
    in.repeat.value = std::numeric_limits<int>::max();
    h.send("a");
  }
  SECTION("maximum integer padding")
  {
    in.pad_left.value = std::numeric_limits<int>::max();
    h.send("");
  }
  h.failure();
  in.replace.value = false;
  in.repeat.value = 3;
  in.pad_left.value = 0;
  h.send("é");
  h.success("ééé"); // exact byte bound, not character count
}

TEST_CASE("Invalid string options emit only an error", "[avnd][string]")
{
  Tool h;
  auto& in = h.object.inputs;
  SECTION("negative repeat")
  {
    in.repeat.value = -1;
  }
  SECTION("negative padding")
  {
    in.pad_right.value = -1;
  }
  SECTION("invalid trim")
  {
    in.trim = static_cast<ao::StringTrim>(99);
  }
  SECTION("invalid case")
  {
    in.casing = static_cast<ao::StringCase>(99);
  }
  SECTION("zero byte limit")
  {
    in.max_bytes.value = 0;
  }
  SECTION("hard byte ceiling")
  {
    in.max_bytes.value = 1048577;
  }
  SECTION("empty pad")
  {
    in.pad_left.value = 1;
    in.pad.value = "";
  }
  SECTION("multi-codepoint pad")
  {
    in.pad_right.value = 1;
    in.pad.value = "e\xCC\x81";
  }
  SECTION("invalid replacement")
  {
    in.replace.value = true;
    in.find.value = "a";
    in.replacement.value = "\xFF";
  }
  SECTION("invalid search")
  {
    in.replace.value = true;
    in.find.value = "\x80";
  }
  SECTION("invalid pad")
  {
    in.pad_left.value = 1;
    in.pad.value = "\xFF";
  }
  h.send("a");
  h.failure();
}

TEST_CASE(
    "Unicode string objects reject malformed UTF-8 rather than corrupting it",
    "[avnd][string]")
{
  const auto malformed = GENERATE(
      std::string{"\x80"}, std::string{"\xC0\xAF"}, std::string{"\xE0\x80\x80"},
      std::string{"\xED\xA0\x80"}, std::string{"\xF0\x80\x80\x80"},
      std::string{"\xF4\x90\x80\x80"}, std::string{"\xF5\x80\x80\x80"},
      std::string{"\xE2\x82"},
      std::string{"\xC2"
                  "A"});
  Tool tool;
  tool.send(malformed);
  tool.failure();
  Split split;
  split.send(malformed);
  split.failure();
  Join join;
  join.send(std::vector<std::string>{"valid", malformed});
  join.failure();
}

TEST_CASE(
    "Unicode scalar boundaries remain valid through split and reverse", "[avnd][string]")
{
  const std::vector<std::string> scalars{
      "\x7F",         "\xC2\x80",         "\xDF\xBF",
      "\xE0\xA0\x80", "\xED\x9F\xBF",     "\xEE\x80\x80",
      "\xEF\xBF\xBF", "\xF0\x90\x80\x80", "\xF4\x8F\xBF\xBF"};
  Join join;
  join.object.inputs.separator.value = "";
  join.send(scalars);
  REQUIRE(join.values.size() == 1);
  Split split;
  split.object.inputs.delimiter.value = "";
  split.send(join.values.front());
  split.success(scalars);
  Tool reverse;
  reverse.object.inputs.reverse.value = true;
  reverse.send(join.values.front());
  REQUIRE(reverse.values.size() == 1);
  split.send(reverse.values.front());
  split.success(std::vector<std::string>{scalars.rbegin(), scalars.rend()});
}

TEST_CASE(
    "String split treats the whole delimiter literally and retains empty fields",
    "[avnd][string]")
{
  Split h;
  h.object.inputs.delimiter.value = "::";
  h.send("::a::::b:c::");
  h.success({"", "a", "", "b:c", ""});
  h.object.inputs.keep_empty.value = false;
  h.send("::a::::b:c::");
  h.success({"a", "b:c"});
  h.send("");
  h.success({});
  h.object.inputs.keep_empty.value = true;
  h.send("");
  h.success({""});
  h.object.inputs.delimiter.value = "東京";
  h.send("é東京😀東京");
  h.success({"é", "😀", ""});
  h.object.inputs.delimiter.value = ".*";
  h.send("a.*b");
  h.success({"a", "b"});
}

TEST_CASE(
    "Empty split delimiter exposes UTF-8 codepoints rather than bytes", "[avnd][string]")
{
  Split h;
  h.object.inputs.delimiter.value = "";
  h.send("aé東😀");
  h.success({"a", "é", "東", "😀"});
  h.send(std::string{"a\0é", 4});
  h.success({"a", std::string(1, '\0'), "é"});
  h.send("");
  h.success({});
  h.object.inputs.delimiter.value = std::string(1, '\0');
  h.send(std::string{"a\0é", 4});
  h.success({"a", "é"});
}

TEST_CASE(
    "String split rejects overflow without emitting a partial list", "[avnd][string]")
{
  Split h;
  auto& in = h.object.inputs;
  SECTION("part count")
  {
    in.max_parts.value = 2;
    h.send("a,b,c");
  }
  SECTION("empty fields count")
  {
    in.max_parts.value = 2;
    h.send(",,");
  }
  SECTION("codepoint count")
  {
    in.max_parts.value = 2;
    in.delimiter.value = "";
    h.send("é東😀");
  }
  SECTION("input bytes")
  {
    in.max_bytes.value = 3;
    h.send("éé");
  }
  SECTION("invalid delimiter")
  {
    in.delimiter.value = "\xFF";
    h.send("a");
  }
  SECTION("zero parts")
  {
    in.max_parts.value = 0;
    h.send("");
  }
  SECTION("hard parts ceiling")
  {
    in.max_parts.value = 65537;
    h.send("");
  }
  SECTION("hard bytes ceiling")
  {
    in.max_bytes.value = 1048577;
    h.send("");
  }
  h.failure();
  in.max_parts.value = 2;
  in.max_bytes.value = 8;
  in.delimiter.value = ",";
  h.send("é,東");
  h.success({"é", "東"});
  in.keep_empty.value = false;
  h.send(",a,,b,");
  h.success({"a", "b"});
}

TEST_CASE(
    "String join preserves empty fields and split-join round trips", "[avnd][string]")
{
  Join h;
  h.send(std::vector<std::string>{});
  h.success("");
  h.send(std::vector<std::string>{""});
  h.success("");
  h.object.inputs.separator.value = "東京";
  h.send(std::vector<std::string>{"", "é", "", "😀", ""});
  h.success("東京é東京東京😀東京");
  Split split;
  split.object.inputs.delimiter.value = "東京";
  const std::string original = h.values.front();
  split.send(original);
  h.send(split.values.front());
  h.success(original);
  h.object.inputs.separator.value = "";
  h.send(std::vector<std::string>{"é", "", "東", "😀"});
  h.success("é東😀");
}

TEST_CASE("String join checks separator, input and total byte growth", "[avnd][string]")
{
  Join h;
  auto& in = h.object.inputs;
  SECTION("separator growth")
  {
    in.max_bytes.value = 3;
    in.separator.value = "é";
    h.send(std::vector<std::string>{"", "", ""});
  }
  SECTION("parts growth")
  {
    in.max_bytes.value = 3;
    h.send(std::vector<std::string>{"é", "é"});
  }
  SECTION("invalid separator")
  {
    in.separator.value = "\xFF";
    h.send(std::vector<std::string>{});
  }
  SECTION("oversize separator")
  {
    in.max_bytes.value = 1;
    in.separator.value = "é";
    h.send(std::vector<std::string>{});
  }
  SECTION("empty parts ceiling")
  {
    h.send(std::vector<std::string>(65537));
  }
  SECTION("invalid limit")
  {
    in.max_bytes.value = 0;
    h.send(std::vector<std::string>{});
  }
  h.failure();
  in.max_bytes.value = 5;
  in.separator.value = ",";
  h.send(std::vector<std::string>{"é", "é"});
  h.success("é,é");
}

TEST_CASE(
    "Raw string byte conversions round trip every byte and UTF-8", "[avnd][string]")
{
  ToBytes encode;
  FromBytes decode;
  std::string binary;
  std::vector<int> integers;
  std::vector<ossia::value> bytes;
  for(int i = 0; i != 256; ++i)
  {
    binary.push_back(std::bit_cast<char>(static_cast<unsigned char>(i)));
    integers.push_back(i);
    bytes.emplace_back(i);
  }
  encode.send(binary);
  encode.success(integers);
  decode.send(bytes);
  decode.success(binary);
  encode.send(std::string{"é\0😀", 7});
  encode.success({195, 169, 0, 240, 159, 152, 128});
  decode.send(std::vector<ossia::value>{195, 169, 0, 240, 159, 152, 128});
  decode.success(std::string{"é\0😀", 7});
  encode.send(std::string{});
  encode.success({});
  decode.send(std::vector<ossia::value>{});
  decode.success("");
}

TEST_CASE(
    "Byte decoding rejects coercion and invalid ranges without partial output",
    "[avnd][string]")
{
  FromBytes h;
  const auto invalid = GENERATE(
      ossia::value{-1}, ossia::value{256}, ossia::value{1.f}, ossia::value{1.5f},
      ossia::value{true}, ossia::value{"42"}, ossia::value{ossia::impulse{}},
      ossia::value{std::vector<ossia::value>{1}}, ossia::value{});
  h.send(std::vector<ossia::value>{65, invalid, 66});
  h.failure();
  h.send(std::vector<ossia::value>{0, 255});
  h.success(std::string{"\0\xFF", 2});
}

TEST_CASE(
    "Raw byte conversion inlets require their actual source types", "[avnd][string]")
{
  ToBytes encode;
  encode.send(ossia::value{65});
  encode.failure();
  encode.send(std::vector<ossia::value>{65});
  encode.failure();
  FromBytes decode;
  decode.send(ossia::value{65});
  decode.failure();
  decode.send(std::string{"65"});
  decode.failure();
  decode.send(ossia::vec2f{65.f, 66.f});
  decode.failure();
}

TEST_CASE(
    "Raw byte conversion bounds allocations and recovers at the limit", "[avnd][string]")
{
  ToBytes encode;
  FromBytes decode;
  SECTION("input above configured budget")
  {
    encode.object.inputs.max_bytes.value = 1;
    decode.object.inputs.max_bytes.value = 1;
  }
  SECTION("zero budget")
  {
    encode.object.inputs.max_bytes.value = 0;
    decode.object.inputs.max_bytes.value = 0;
  }
  SECTION("above hard ceiling")
  {
    encode.object.inputs.max_bytes.value = 1048577;
    decode.object.inputs.max_bytes.value = 1048577;
  }
  encode.send(std::string{"\0\xFF", 2});
  encode.failure();
  decode.send(std::vector<ossia::value>{0, 255});
  decode.failure();
  encode.object.inputs.max_bytes.value = 2;
  decode.object.inputs.max_bytes.value = 2;
  encode.send(std::string{"\0\xFF", 2});
  encode.success({0, 255});
  decode.send(std::vector<ossia::value>{0, 255});
  decode.success(std::string{"\0\xFF", 2});
}
