// "Restart at bar lines" on a synced time chooser: the second component of
// the value is the mode, 1 leaving the bar lines to the object, 2 restarting
// the steps at each of them, 3 keeping one pulse across them. Which objects
// offer it, what 1 means for them, the decoding the binding does, the menu
// and its command, and the modes through a save and a load, a value saved
// before the choice existed (1) included.

#include <Process/Dataflow/ControlWidgets.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>

#include <score/graphics/widgets/QGraphicsTimeChooser.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <avnd/binding/ossia/time_controls.hpp>

#include <QAction>
#include <QMenu>

#include <Fx/Arpeggiator_v2.hpp>
#include <Fx/Metro_v2.hpp>
#include <Fx/RateLimiter_v2.hpp>
#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Process.hpp>
#include <score_test/Project.hpp>

#include <memory>

namespace
{
const QString arp_uuid = QStringLiteral("4df48c1c-f5f2-4ab1-9f37-3b0adba139ae");
const QString metro_uuid = QStringLiteral("6e001674-d4a5-478f-9f7e-d7ad1bdcee2b");
const QString limiter_uuid = QStringLiteral("485d39ed-e495-448c-95df-5f2ea905ef5b");

using ArpRate = decltype(Nodes::Arpeggiator::v2::Node::ins::rate);
using LimiterInterval = decltype(Nodes::RateLimiter::v2::Node::ins::interval);

Process::ProcessModel* roundtrip_datastream(
    const Process::ProcessModel& proc, const score::DocumentContext& dctx,
    QObject* parent)
{
  auto& pl = dctx.app.interfaces<Process::ProcessFactoryList>();
  const QByteArray bytes = DataStreamReader::marshall(proc);
  DataStream::Deserializer des{bytes};
  return deserialize_interface(pl, des, dctx, parent);
}

Process::ProcessModel* roundtrip_json(
    const Process::ProcessModel& proc, const score::DocumentContext& dctx,
    QObject* parent)
{
  auto& pl = dctx.app.interfaces<Process::ProcessFactoryList>();
  JSONReader reader;
  reader.readFrom(proc);
  const auto doc = readJson(reader.toByteArray());
  JSONObject::Deserializer des{doc};
  return deserialize_interface(pl, des, dctx, parent);
}

Process::TimeChooser& time_chooser(Process::ProcessModel& p, const QString& name)
{
  auto* t = qobject_cast<Process::TimeChooser*>(&score::test::control_named(p, name));
  REQUIRE(t);
  return *t;
}

ossia::vec2f value_of(const Process::ControlInlet& c)
{
  return ossia::convert<ossia::vec2f>(c.value());
}
}

TEST_CASE("time chooser bar sync: the binding reads the mode", "[time_chooser][avnd]")
{
  // Declared by the arpeggiator: one pulse unless asked otherwise
  static_assert(ArpRate::offers_bar_sync && !ArpRate::bar_sync_default);
  CHECK(!oscr::bar_sync_of<ArpRate>(1.f));
  CHECK(oscr::bar_sync_of<ArpRate>(2.f));
  CHECK(!oscr::bar_sync_of<ArpRate>(3.f));
  // Declared by the rate limiter: the bar grid unless asked otherwise
  static_assert(LimiterInterval::offers_bar_sync && LimiterInterval::bar_sync_default);
  CHECK(oscr::bar_sync_of<LimiterInterval>(1.f));
  CHECK(oscr::bar_sync_of<LimiterInterval>(2.f));
  CHECK(!oscr::bar_sync_of<LimiterInterval>(3.f));
  // Not declared: no choice, the bar grid
  using Plain = halp::time_chooser<"Time">;
  static_assert(!Plain::offers_bar_sync && Plain::bar_sync_default);
  CHECK(oscr::bar_sync_of<Plain>(1.f));
  // A mode from nowhere reads as 1
  CHECK(!oscr::bar_sync_of<ArpRate>(0.5f));
  CHECK(!oscr::bar_sync_of<ArpRate>(7.f));
}

TEST_CASE("time chooser bar sync: the ports offer what the object declares", "[time_chooser]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* arp = score::test::add_process(*doc, arp_uuid, {});
    auto* metro = score::test::add_process(*doc, metro_uuid, {});
    auto* limiter = score::test::add_process(*doc, limiter_uuid, {});
    if(!arp || !metro || !limiter)
      SKIP("the fx plug-in is not built");

    auto& rate = time_chooser(*arp, "Rate");
    CHECK(rate.offersBarSync);
    CHECK(!rate.barSyncDefault);
    auto& interval = time_chooser(*limiter, "Interval");
    CHECK(interval.offersBarSync);
    CHECK(interval.barSyncDefault);
    CHECK(!time_chooser(*metro, "Period").offersBarSync);
  });
}

TEST_CASE("time chooser bar sync: the mode is saved and loaded as it is", "[time_chooser]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    auto* arp = score::test::add_process(*doc, arp_uuid, {});
    if(!arp)
      SKIP("the fx plug-in is not built");
    auto& rate = time_chooser(*arp, "Rate");

    // 1 is what a value saved before the choice existed holds: it stays 1, and
    // keeps meaning the object's choice
    for(float mode : {1.f, 2.f, 3.f, 0.f})
      for(auto roundtrip : {&roundtrip_json, &roundtrip_datastream})
      {
        INFO("mode " << mode << (roundtrip == &roundtrip_json ? " json" : " binary"));
        const ossia::vec2f v{mode == 0.f ? 0.4f : 3.f / 32.f, mode};
        rate.setValue(v);
        std::unique_ptr<Process::ProcessModel> loaded{
            roundtrip(*arp, dctx, &doc->model())};
        REQUIRE(loaded);
        auto& again = time_chooser(*loaded, "Rate");
        CHECK(value_of(again) == v);
        CHECK(again.offersBarSync);
        CHECK(!again.barSyncDefault);
      }
  });
}

TEST_CASE("time chooser bar sync: the menu entry is one undoable edit", "[time_chooser]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* arp = score::test::add_process(*doc, arp_uuid, {});
    auto* metro = score::test::add_process(*doc, metro_uuid, {});
    if(!arp || !metro)
      SKIP("the fx plug-in is not built");
    auto& rate = time_chooser(*arp, "Rate");
    rate.setValue(ossia::vec2f{3.f / 32.f, 1.f}); // 1/16., as saved before

    auto& stack = doc->commandStack();
    QObject context;
    std::unique_ptr<score::QGraphicsTimeChooser> item{
        WidgetFactory::TimeChooser::make_item(rate, rate, doc->context(), nullptr, &context)};
    REQUIRE(item->offersBarSync);
    CHECK(!item->barSync());

    const auto entry = [&] {
      auto* menu = item->makeSyncMenu();
      REQUIRE(menu->actions().size() == 1);
      auto* act = menu->actions()[0];
      CHECK(act->text() == QStringLiteral("Restart at bar lines"));
      CHECK(act->isCheckable());
      return std::pair{std::unique_ptr<QMenu>{menu}, act};
    };

    const int base = stack.size();
    {
      auto [menu, act] = entry();
      CHECK(!act->isChecked());
      act->trigger();
    }
    CHECK(stack.size() == base + 1);
    CHECK(value_of(rate) == ossia::vec2f{3.f / 32.f, 2.f});
    CHECK(item->barSync());
    {
      auto [menu, act] = entry();
      CHECK(act->isChecked());
      act->trigger();
    }
    CHECK(stack.size() == base + 2);
    CHECK(value_of(rate) == ossia::vec2f{3.f / 32.f, 3.f});
    CHECK(!item->barSync());

    stack.undo();
    CHECK(value_of(rate) == ossia::vec2f{3.f / 32.f, 2.f});
    CHECK(item->barSync());
    stack.undo();
    CHECK(value_of(rate) == ossia::vec2f{3.f / 32.f, 1.f});
    CHECK(!item->barSync());

    // Free and back: the choice comes back with the note value
    rate.setValue(ossia::vec2f{3.f / 32.f, 2.f});
    item->cycleMode(); // 1/16. to 1/16T
    item->cycleMode(); // to free
    CHECK(value_of(rate)[1] == 0.f);
    item->cycleMode(); // to 1/16, straight
    CHECK(value_of(rate)[1] == 2.f);

    // An object that does not read it has no menu on a synced time
    auto& period = time_chooser(*metro, "Period");
    period.setValue(ossia::vec2f{1.f / 8.f, 1.f});
    std::unique_ptr<score::QGraphicsTimeChooser> plain{WidgetFactory::TimeChooser::make_item(
        period, period, doc->context(), nullptr, &context)};
    CHECK(!plain->offersBarSync);
  });
}
