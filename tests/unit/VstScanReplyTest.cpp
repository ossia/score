// vst::parseVstReply — the VST2 half of the scan-reply parsing.

#include <Vst/ApplicationPlugin.hpp>

#include <QJsonDocument>
#include <QJsonObject>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("parseVstReply fills VSTInfo from a puppet reply", "[pluginscan][vst]")
{
  const auto doc = QJsonDocument::fromJson(R"({
    "UniqueID": 1450406192,
    "Controls": 12,
    "Author": "The \"Best\" Vendor",
    "PrettyName": "SuperSynth",
    "Version": "1.2.3",
    "Synth": true,
    "Path": "/some/other/claimed/path.so",
    "Request": 3,
    "Token": "tok"
  })");
  REQUIRE(doc.isObject());

  const auto info = vst::parseVstReply("/usr/lib/vst/supersynth.so", doc.object());

  CHECK(info.uniqueID == 1450406192);
  CHECK(info.controls == 12);
  CHECK(info.author == "The \"Best\" Vendor");
  CHECK(info.displayName == "SuperSynth");
  CHECK(info.isSynth);
  CHECK(info.isValid);
  // The scanned path wins over whatever the reply claims
  CHECK(info.path == "/usr/lib/vst/supersynth.so");
  // Pretty name comes from the file name (multi-variant plug-ins)
  CHECK(info.prettyName == "supersynth");
}
