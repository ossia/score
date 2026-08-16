// Tests for the shared puppet-side helpers (score/tools/PuppetJson.hpp):
//
//  * JSON string escaping: plug-in metadata (names, vendors, descriptions)
//    routinely contains quotes and other JSON-hostile characters; before
//    the escaping was introduced, a single such plug-in silently broke its
//    whole scan reply (the host dropped the unparseable JSON, timed out,
//    and marked the plug-in invalid).
//  * Command-line parsing for the token/port scan protocol, including the
//    legacy `puppet <path> <id>` form that must keep working.

#include <score/tools/PuppetJson.hpp>

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <catch2/catch_test_macros.hpp>

using score::puppet::json_escape;
using score::puppet::parse_arguments;

TEST_CASE("json_escape leaves plain text alone", "[puppet][json]")
{
  REQUIRE(json_escape("Diode Ladder Filter") == "Diode Ladder Filter");
  REQUIRE(json_escape("") == "");
  REQUIRE(json_escape((const char*)nullptr) == "");
}

TEST_CASE("json_escape escapes JSON-hostile characters", "[puppet][json]")
{
  REQUIRE(json_escape(R"(The "best" plugin)") == R"(The \"best\" plugin)");
  REQUIRE(json_escape(R"(C:\Plugins\foo.dll)") == R"(C:\\Plugins\\foo.dll)");
  REQUIRE(json_escape("line1\nline2") == R"(line1\nline2)");
  REQUIRE(json_escape("a\tb") == R"(a\tb)");
  REQUIRE(json_escape("a\rb") == R"(a\rb)");
  REQUIRE(json_escape("a\bb") == R"(a\bb)");
  REQUIRE(json_escape("a\fb") == R"(a\fb)");
  REQUIRE(json_escape(std::string_view{"\x01\x1f", 2}) == R"(\u0001\u001f)");
}

TEST_CASE("json_escape passes UTF-8 through untouched", "[puppet][json]")
{
  REQUIRE(json_escape("Frédéric — 変調") == "Frédéric — 変調");
}

TEST_CASE("escaped strings survive a real JSON parser roundtrip", "[puppet][json]")
{
  // The actual regression: build a reply the way the puppets do, then parse
  // it the way the ApplicationPlugins do.
  const std::string hostile = "A \"plugin\" with\nnewlines & \\backslashes\\";
  const std::string reply = "{\"Name\":\"" + json_escape(hostile) + "\"}";

  QJsonParseError err{};
  const auto doc
      = QJsonDocument::fromJson(QByteArray::fromStdString(reply), &err);
  REQUIRE(err.error == QJsonParseError::NoError);
  REQUIRE(doc.isObject());
  REQUIRE(doc.object()["Name"].toString().toStdString() == hostile);
}

TEST_CASE("parse_arguments: full protocol form", "[puppet][args]")
{
  const char* argv[] = {"puppet", "/usr/lib/vst3/Foo.vst3", "7", "43210", "s3cr3t"};
  auto args = parse_arguments(5, const_cast<char**>(argv), 37588);
  REQUIRE(args.valid);
  REQUIRE(args.path == "/usr/lib/vst3/Foo.vst3");
  REQUIRE(args.request_id == 7);
  REQUIRE(args.port == 43210);
  REQUIRE(args.token == "s3cr3t");
}

TEST_CASE("parse_arguments: legacy forms fall back to defaults", "[puppet][args]")
{
  SECTION("path + id (pre-token hosts)")
  {
    const char* argv[] = {"puppet", "/plug.so", "3"};
    auto args = parse_arguments(3, const_cast<char**>(argv), 37587);
    REQUIRE(args.valid);
    REQUIRE(args.request_id == 3);
    REQUIRE(args.port == 37587);
    REQUIRE(args.token.empty());
  }
  SECTION("path only (manual debugging)")
  {
    const char* argv[] = {"puppet", "/plug.so"};
    auto args = parse_arguments(2, const_cast<char**>(argv), 37587);
    REQUIRE(args.valid);
    REQUIRE(args.request_id == 0);
    REQUIRE(args.port == 37587);
  }
  SECTION("no arguments is invalid")
  {
    const char* argv[] = {"puppet"};
    REQUIRE(!parse_arguments(1, const_cast<char**>(argv), 37587).valid);
  }
}

TEST_CASE("parse_arguments: garbage numbers do not misroute the reply", "[puppet][args]")
{
  const char* argv[] = {"puppet", "/plug.so", "banana", "0", "tok"};
  auto args = parse_arguments(5, const_cast<char**>(argv), 37587);
  REQUIRE(args.valid);
  REQUIRE(args.request_id == 0); // not atoi-garbage
  REQUIRE(args.port == 37587);   // port 0 rejected, default kept
  REQUIRE(args.token == "tok");
}
