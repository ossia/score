// vst3::parseVst3Reply — the VST3 half of the scan-reply parsing.
//
// Regression: classFlags used to be read from the "Version" key (a string,
// so QJsonValue::toDouble() returned 0.0) — every cached VST3 class had
// classFlags == 0 regardless of what the plug-in declared.

#include <Vst3/ApplicationPlugin.hpp>

#include <QJsonDocument>
#include <QJsonObject>

#include <catch2/catch_test_macros.hpp>

namespace
{
QJsonObject parse(const char* json)
{
  const auto doc = QJsonDocument::fromJson(json);
  REQUIRE(doc.isObject());
  return doc.object();
}
}

TEST_CASE("parseVst3Reply fills classes from a puppet reply", "[pluginscan][vst3]")
{
  const auto obj = parse(R"({
    "Name": "MegaPlugin",
    "Url": "https://example.com",
    "Path": "/claimed/elsewhere.vst3",
    "Request": 2,
    "Token": "tok",
    "Classes": [{
      "UID": "5BC32507D06049EA865193447511E8A2",
      "Cardinality": 2147483647,
      "Category": "Audio Module Class",
      "Name": "Mega Reverb",
      "Vendor": "ACME",
      "Version": "1.0.2",
      "SDKVersion": "VST 3.7.0",
      "Subcategories": "Fx|Reverb",
      "ClassFlags": 17
    }]
  })");

  const auto info = vst3::parseVst3Reply("/usr/lib/vst3/mega.vst3", obj);

  REQUIRE(info.isValid);
  CHECK(info.path == "/usr/lib/vst3/mega.vst3"); // scanned path wins
  CHECK(info.name == "MegaPlugin");
  REQUIRE(info.classInfo.size() == 1);

  const auto& cls = info.classInfo[0];
  CHECK(cls.name() == "Mega Reverb");
  CHECK(cls.vendor() == "ACME");
  CHECK(cls.version() == "1.0.2");
  CHECK(cls.subCategories().size() == 2);
  CHECK(cls.subCategories()[0] == "Fx");
  CHECK(cls.subCategories()[1] == "Reverb");
  // The regression: this was parsed from "Version" and always came out 0
  CHECK(cls.classFlags() == 17);
}

TEST_CASE("parseVst3Reply: no classes means not loadable", "[pluginscan][vst3]")
{
  const auto info = vst3::parseVst3Reply(
      "/usr/lib/vst3/empty.vst3",
      parse(R"({"Name":"Empty","Request":0,"Token":"t","Classes":[]})"));
  CHECK(!info.isValid);
}

TEST_CASE("parseVst3Reply skips malformed class UIDs", "[pluginscan][vst3]")
{
  const auto info = vst3::parseVst3Reply("/p.vst3", parse(R"({
    "Name": "Broken", "Request": 0, "Token": "t",
    "Classes": [
      {"UID": "not-a-uid", "Name": "Bad"},
      {"UID": "5BC32507D06049EA865193447511E8A2", "Name": "Good",
       "ClassFlags": 1, "Subcategories": "Fx"}
    ]
  })"));

  REQUIRE(info.isValid);
  REQUIRE(info.classInfo.size() == 1);
  CHECK(info.classInfo[0].name() == "Good");
}
