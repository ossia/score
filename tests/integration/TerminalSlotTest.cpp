// What a terminal draws of the score's structure.
//
// The base interval's full view is the whole editing surface. If a process
// added on the machine running the score does not get a slot here, the person
// at the terminal cannot see or select it -- and there is then no inspector to
// set its ports from.

#include <Process/ProcessList.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Interval/FullView/FullViewIntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>

#include <score/document/DocumentRole.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
const UuidKey<Process::ProcessModel> automationKey{
    score::uuids::string_generator::compute("d2a67bd8-5d3f-404e-b6e9-e350cf2a833f")};

Scenario::IntervalModel& baseInterval(score::Document& doc)
{
  return safe_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate())
      .baseScenario()
      .interval();
}

//! The number of slots the full view actually built, which is what is drawn.
std::size_t displayedSlots(const score::GUIApplicationContext& ctx, score::Document& doc)
{
  ctx.docManager.setCurrentDocument(ctx, &doc);
  QApplication::processEvents();

  auto* pres
      = safe_cast<Scenario::ScenarioDocumentPresenter*>(doc.presenter()->presenterDelegate());
  SCORE_ASSERT(pres);

  // Loading builds the presenter but not the layers; this is what the
  // application does on opening a score.
  pres->setDisplayedInterval(&baseInterval(doc));
  QApplication::processEvents();

  auto* full
      = dynamic_cast<Scenario::FullViewIntervalPresenter*>(pres->displayedIntervalPresenter());
  SCORE_ASSERT(full);
  return full->getSlots().size();
}

QByteArray asJson(score::Document& doc)
{
  JSONObject::Serializer wr{};
  doc.saveAsJson(wr);
  return wr.toByteArray();
}

score::Document* reload(
    const score::GUIApplicationContext& ctx, const QByteArray& bytes,
    score::DocumentRole role)
{
  auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
  SCORE_ASSERT(!delegates.empty());
  auto* doc = ctx.docManager.loadDocument(
      ctx, QStringLiteral("slots"), bytes, JSONObject::type(), *delegates.begin(), role);
  QApplication::processEvents();
  return doc;
}
}

TEST_CASE("A terminal draws the same slots as the machine running the score", "[terminal]")
{
  qputenv("SCORE_DISABLE_LIBRARY", "1");

  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto& facs = ctx.interfaces<Process::ProcessFactoryList>();
    REQUIRE(facs.get(automationKey));

    auto* origin = score::test::new_document(ctx);
    REQUIRE(origin);

    // A process on the base interval, put there the way the application and
    // the scripting API both do it.
    const auto before = displayedSlots(ctx, *origin);
    {
      Scenario::Command::Macro m{
          new Scenario::Command::AddProcessInNewBoxMacro, origin->context()};
      REQUIRE(m.createProcessInNewSlot(baseInterval(*origin), automationKey, {}));
      m.commit();
    }
    QApplication::processEvents();
    REQUIRE(baseInterval(*origin).processes.size() == 2);

    // The precondition: a document that runs here draws a slot for it. If this
    // is one, the full view never shows base-interval processes and there is
    // nothing for a terminal to be missing.
    const auto local = displayedSlots(ctx, *origin);
    CHECK(local == before + 1);

    // The same score, opened as a terminal: the processes belong to the other
    // machine but the structure is what is being edited here.
    const auto bytes = asJson(*origin);
    auto* terminal = reload(ctx, bytes, score::DocumentRole::Terminal);
    REQUIRE(terminal);
    REQUIRE(baseInterval(*terminal).processes.size() == 2);

    CHECK(displayedSlots(ctx, *terminal) == local);
  });
}
