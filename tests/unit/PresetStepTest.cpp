// The preset button's Forward / Back mouse buttons step through a process's
// presets in the order its menu lists them, from the preset loaded last.
//
// Stepping found the "current" preset by matching the process's name, on the
// belief that loading a preset renames the process. Nothing does, so no preset
// ever matched: Forward always loaded the first preset and Back the last, and
// with presets spread over categories only those two were ever reachable. Its
// order was also a flat sort of the raw category strings, while the menu nests
// trimmed path segments, so the two could disagree.

#include <Process/ApplicationPlugin.hpp>
#include <Process/Preset.hpp>
#include <Process/Process.hpp>

#include <Effect/EffectLayer.hpp>

#include <score/graphics/widgets/QGraphicsSelectablePixmapToggle.hpp>

#include <core/document/Document.hpp>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <memory>
#include <vector>

namespace
{
// Pattern sequencer: fixed controls, presets of its own in the library
const QString pattern_uuid = QStringLiteral("49047204-5c1e-4b54-9f43-2b583f664b2e");

QString current(const Process::ProcessModel& proc)
{
  const auto c = proc.property("score_presetCursor").toStringList();
  return c.size() == 2 ? c[1] : QString{};
}
}

TEST_CASE("the preset button steps through the presets in the menu's order", "[presets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = score::test::add_process(*doc, pattern_uuid, {});
    REQUIRE(proc);

    // Only the presets of this test for this process
    auto& pplug = ctx.applicationPlugin<Process::ApplicationPlugin>();
    const auto library = pplug.presets;
    std::erase_if(pplug.presets, [&](const auto& p) { return proc->presetMatches(p); });

    const auto saved = proc->savePreset();
    auto preset = [&](QString category, QString name) {
      auto p = saved;
      p.category = std::move(category);
      p.name = std::move(name);
      return p;
    };
    // Out of order; "A/B" written two ways; "A-C" sorts between "A" and
    // "A/B" as a string ('-' < '/') but is a sibling of "A" in the menu
    for(auto&& p :
        {preset("Z", "z1"), preset("A-C", "ac1"), preset(" A / B ", "ab2"),
         preset("A", "a2"), preset("", "root1"), preset("A/B", "ab1"),
         preset("A", "a1")})
      pplug.presets.push_back(p);

    // Root first, then each submenu depth-first, as the menu shows them; the
    // process's built-in presets come after the library's
    std::vector<QString> expected{"root1", "a1", "a2", "ab1", "ab2", "ac1", "z1"};
    for(auto& p : proc->builtinPresets())
      expected.push_back(p.name);
    const int n = int(expected.size());

    auto btn = std::unique_ptr<score::QGraphicsDraggablePixmap>(
        Process::makePresetButton(*proc, doc->context(), nullptr, nullptr));
    REQUIRE(btn);
    REQUIRE(btn->click);

    // A name that is a preset's says nothing: loading does not rename
    proc->metadata().setName("z1");

    for(int i = 0; i < n; i++)
    {
      btn->click(Qt::ForwardButton, {});
      INFO("step " << i);
      CHECK(current(*proc) == expected[std::size_t(i)]);
    }
    // Wraps around, both ways
    btn->click(Qt::ForwardButton, {});
    CHECK(current(*proc) == expected[0]);
    btn->click(Qt::BackButton, {});
    CHECK(current(*proc) == expected[std::size_t(n - 1)]);
    btn->click(Qt::BackButton, {});
    CHECK(current(*proc) == expected[std::size_t(n - 2)]);

    // The current preset belongs to the process, not to the button: headers
    // are rebuilt all the time
    btn = std::unique_ptr<score::QGraphicsDraggablePixmap>(
        Process::makePresetButton(*proc, doc->context(), nullptr, nullptr));
    btn->click(Qt::ForwardButton, {});
    CHECK(current(*proc) == expected[std::size_t(n - 1)]);

    pplug.presets = library;
  });
}
