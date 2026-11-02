// A preset file keeps a process's data as it is: JSON as JSON, anything else
// (a plug-in's own format) byte for byte rather than as null.

#include <Process/Preset.hpp>
#include <Process/ProcessList.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

namespace
{
// LFO
const auto lfo = UuidKey<Process::ProcessModel>::fromString(
    QStringLiteral("0b1b1816-c33e-4796-a16d-5aab27fe600f"));

Process::Preset preset(QByteArray data)
{
  Process::Preset p;
  p.name = "p";
  p.category = "c";
  p.key = {lfo, "fx"};
  p.data = std::move(data);
  return p;
}
}

TEST_CASE("a preset's data survives its file, JSON or not", "[presets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
    if(!procs.get(lfo))
      SKIP("Fx not built");

    for(const QByteArray& data :
        {QByteArray(R"({"Controls":[1,2]})"), QByteArray("[]"),
         QByteArray("urn:not json\x01\x00\xff", 17), QByteArray("{broken")})
    {
      INFO(data.toStdString());
      const auto json = preset(data).toJson();
      CHECK(!json.contains("\"Preset\":null"));
      auto back = Process::Preset::fromJson(procs, json);
      REQUIRE(back);
      CHECK(back->name == "p");
      CHECK(back->category == "c");
      CHECK(back->key.effect == "fx");
      if(data.startsWith('{') && !data.startsWith("{broken"))
        CHECK(back->data == R"({"Controls":[1,2]})");
      else
        CHECK(back->data == data);
    }
  });
}

TEST_CASE("a preset's name and category are kept in any script", "[presets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
    if(!procs.get(lfo))
      SKIP("Fx not built");

    auto p = preset(QByteArray("[]"));
    p.name = QStringLiteral("鼓声 ü 🥁");
    p.category = QStringLiteral("打击乐/低音");
    p.description = QStringLiteral("一个预设");
    auto back = Process::Preset::fromJson(procs, p.toJson());
    REQUIRE(back);
    CHECK(back->name == p.name);
    CHECK(back->category == p.category);
    CHECK(back->description == p.description);
  });
}
