// A JSFX process loaded with its document is its script as when it was added:
// the preset bank next to the script's file comes with it.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <YSFX/ProcessModel.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
const QString ysfx_uuid = QStringLiteral("cb6b50d3-82c0-4356-a282-317da8b8022f");

constexpr auto script = R"(desc:bank test
slider1:0<0,10,1>Level
out_pin:out
@sample
spl0 = slider1;
)";

YSFX::ProcessModel* find_ysfx(score::Document& doc)
{
  for(auto& p : score::test::base_interval(doc).processes)
    if(auto* y = qobject_cast<YSFX::ProcessModel*>(&p))
      return y;
  return nullptr;
}

//! A bank of two presets next to the script, written by ysfx itself
void write_bank(const QString& script_path)
{
  ysfx_config_u config{ysfx_config_new()};
  ysfx_u fx{ysfx_new(config.get())};
  const auto path = script_path.toStdString();
  REQUIRE(ysfx_load_file(fx.get(), path.c_str(), 0));
  REQUIRE(ysfx_compile(fx.get(), 0));
  ysfx_init(fx.get());

  ysfx_bank_t* bank = ysfx_create_empty_bank("bank test");
  for(auto [name, level] : {std::pair{"Low", 2.}, std::pair{"High", 8.}})
  {
    ysfx_slider_set_value(fx.get(), 0, level, false);
    // The bank it makes takes the state
    ysfx_bank_t* next = ysfx_add_preset_to_bank(bank, name, ysfx_save_state(fx.get()));
    ysfx_bank_free(bank);
    bank = next;
  }
  REQUIRE(ysfx_save_bank((path + ".rpl").c_str(), bank));
  ysfx_bank_free(bank);
}
}

TEST_CASE(
    "a JSFX process keeps the presets of its script once loaded with its document",
    "[integration][ysfx][presets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("bank.jsfx");
    score::test::write_file(path, script);
    write_bank(path);

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = static_cast<YSFX::ProcessModel*>(
        score::test::add_process(*doc, ysfx_uuid, path));
    if(!proc || !proc->fx)
      SKIP("YSFX not built");
    const Process::ProcessModel& base = *proc;
    REQUIRE(base.builtinPresets().size() == 2);

    for(auto* loaded : {score::test::reload_via_json(ctx, *doc),
                        score::test::reload_via_bytes(ctx, *doc)})
    {
      REQUIRE(loaded);
      auto* p = find_ysfx(*loaded);
      REQUIRE(p);
      REQUIRE(p->fx);
      const Process::ProcessModel& b = *p;
      const auto presets = b.builtinPresets();
      REQUIRE(presets.size() == 2);
      CHECK(presets[0].name == "Low");
      CHECK(presets[1].name == "High");
    }
  });
}
