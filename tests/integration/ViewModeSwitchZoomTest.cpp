// Temporal -> nodal -> temporal: the interval is drawn at the zoom the view
// uses. An interval shorter than the view is fitted to it, which differs from
// its stored zoom; the second temporal display must use the fitted zoom too,
// or it stops part of the way across the view while the ruler and bar lines go
// on.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Events.hpp>

#include <Scenario/Document/Interval/FullView/FullViewIntervalView.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>

#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QGraphicsView>

#include <catch2/catch_test_macros.hpp>

namespace
{
double intervalWidth(Scenario::ScenarioDocumentPresenter& p)
{
  for(auto* it : p.view().scene().items())
    if(it->type() == Scenario::FullViewIntervalView::Type)
      return it->sceneBoundingRect().width();
  return -1.;
}
}

TEST_CASE(
    "The interval keeps the view's zoom across temporal / nodal switches",
    "[integration][gui][zoom]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto p = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(*doc);
    REQUIRE(p);
    {
      QWidget* w = &p->view().view();
      while(w->parentWidget())
        w = w->parentWidget();
      w->resize(1800, 1100);
      w->show();
    }
    const auto view = [&] { return p->view().viewportRect().width(); };
    REQUIRE(score::test::wait_until([&] { return intervalWidth(*p) > 0. && view() > 0.; }));

    // A stored zoom at which the interval is much shorter than the view.
    auto& itv = p->displayedInterval();
    const auto dur = itv.duration.guiDuration();
    itv.setZoom(dur.impl / 400.);

    const auto roundTrip = [&] {
      p->setNodalMode(true);
      score::test::settle();
      p->setNodalMode(false);
      score::test::wait_until([&] { return intervalWidth(*p) >= 0.95 * view(); }, 3000);
      return intervalWidth(*p);
    };
    const double first = roundTrip();
    REQUIRE(first > 0.);
    const double second = roundTrip();

    INFO("view " << view() << ", first " << first << ", second " << second);
    CHECK(second >= first - 2.);
    CHECK(second >= 0.95 * view());
  });
}
