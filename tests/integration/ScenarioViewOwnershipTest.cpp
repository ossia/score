// Integration test for 6227fbd07f "scenario: stop Qt deleting by-value
// members of ScenarioDocumentView".
//
// The bug family: an object held BY VALUE inside ScenarioDocumentView is also
// handed to Qt's parent/child ownership, so Qt teardown calls delete on an
// address that was never its own heap allocation. Concretely, m_scene and
// m_minimapScene were constructed with m_widget as their QObject parent, so
// ~QWidget's deleteChildren() freed them out from under the view; and
// m_baseObject was a by-value member added to a scene that deletes its
// top-level items.
//
// Why this is asserted STRUCTURALLY rather than by crashing: whether the
// double-ownership actually detonates depends on destruction ORDER. The app
// destroys the widget hierarchy while the delegate view is still alive
// (crash); the test fixtures destroy the delegate view first, at which point
// the by-value scenes have already unregistered from the widget's child list
// (silence). The invariant the fix establishes is order-independent and
// directly observable: NOTHING Qt owns through the widget may be a by-value
// member of the view. So:
//
//   - the widget has no QGraphicsScene among its direct QObject children
//     (pre-fix it had two: m_scene and m_minimapScene);
//   - the base item is heap-allocated and owned by the scene it lives in
//     (top-level, no parent item), which is the one party that deletes it.
//
// 6227fbd07f deliberately did NOT fix the same shape for the by-value QWidget
// members that a layout reparents (m_view, m_timeRulerView, m_minimapView,
// m_minimap) -- that was left to its own change, pinned expected-red below so
// the remainder could not be forgotten. That refactor has now landed and the
// pin is green; see the second case for what it actually asserts and why the
// pin's original assertion could never have detected the fix.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/Minimap/Minimap.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>

#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QWidget>

#include <catch2/catch_test_macros.hpp>

namespace
{
Scenario::ScenarioDocumentView& viewOf(score::Document& doc)
{
  auto p
      = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(
          doc);
  REQUIRE(p != nullptr);
  return p->view();
}
}

TEST_CASE(
    "Qt owns no by-value scene of the scenario document view",
    "[integration][scenario][ownership][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& view = viewOf(*doc);
    QWidget* w = view.getWidget();
    REQUIRE(w != nullptr);

    // The two scenes are by-value members of the view. If either were a Qt
    // child of the widget, ~QWidget would delete an address inside the view
    // object -- the exact teardown AddressSanitizer flagged. m_timeRulerScene
    // always did this correctly, so the correct count is zero, not one.
    CHECK(
        w->findChild<QGraphicsScene*>(QString(), Qt::FindDirectChildrenOnly)
        == nullptr);
    // And not through any intermediate widget either.
    CHECK(w->findChild<QGraphicsScene*>() == nullptr);

    // The scenes exist and work; they are just not Qt-owned.
    CHECK(view.scene().parent() == nullptr);

    // The base item is a top-level item of the scene, which deletes its
    // top-level items -- so it must be heap-allocated (the fix) and owned by
    // exactly that scene.
    CHECK(view.baseItem().scene() == &view.scene());
    CHECK(view.baseItem().parentItem() == nullptr);

    // Document teardown after this lambda (document_closer) is the
    // crash-free-close half of the assertion.
  });
}

// The remainder of 6227fbd07f, now fixed: ProcessGraphicsView m_view,
// TimeRulerGraphicsView m_timeRulerView, MinimapGraphicsView m_minimapView and
// Minimap m_minimap were by-value members that a layout reparents into the
// widget Qt deletes, so ~QWidget called delete on addresses inside the view
// object. They are heap-allocated now.
//
// THE PIN'S OWN ASSERTION WAS WRONG and is replaced. It read
//
//     const auto* qtOwned = view.getWidget()->findChild<QGraphicsView*>();
//     CHECK((const void*)qtOwned != (const void*)&view.view());
//
// but `view.view()` returns *m_view whichever way m_view is stored, so
// findChild and the accessor name the same object before AND after the
// refactor: the comparison is false either way and the case could not have
// gone green when the defect was fixed. Worse, it WOULD have gone green for a
// purely cosmetic member reordering that made m_minimapView the first
// QGraphicsView child, with no ownership change at all.
//
// What the pin's prose asked for is order-independent and directly testable:
// nothing Qt owns through the widget may live at an address inside the view
// object. That is what is asserted below -- for every QWidget and every
// QGraphicsItem Qt can reach, not just the first graphics view.
TEST_CASE(
    "no widget or item Qt owns lives inside the scenario document view",
    "[integration][scenario][ownership][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& view = viewOf(*doc);
    QWidget* w = view.getWidget();
    REQUIRE(w != nullptr);

    // The byte range of the ScenarioDocumentView object itself. Anything Qt
    // will `delete` must lie outside it.
    const auto* lo = reinterpret_cast<const char*>(&view);
    const auto* hi = lo + sizeof(Scenario::ScenarioDocumentView);
    const auto inside = [lo, hi](const void* p) {
      const auto* c = reinterpret_cast<const char*>(p);
      return c >= lo && c < hi;
    };

    // Sanity: the range test can actually catch something. `view` itself is
    // inside it by construction, and a fresh heap allocation is not.
    REQUIRE(inside(&view));
    auto* heap = new QWidget;
    REQUIRE_FALSE(inside(heap));
    delete heap;

    // Every widget Qt reaches through the document's widget, at any depth.
    const auto widgets = w->findChildren<QWidget*>();
    INFO("widgets Qt owns under the document widget: " << widgets.size());
    REQUIRE(widgets.size() >= 3); // the three views, at least
    for(const QWidget* child : widgets)
    {
      INFO("child: " << child->metaObject()->className());
      CHECK_FALSE(inside(child));
    }

    // The three the pin named, by identity rather than by search order.
    CHECK_FALSE(inside(&view.view()));
    CHECK_FALSE(inside(&view.rulerView()));
    CHECK_FALSE(inside(&view.minimap()));

    // A QGraphicsScene deletes its top-level items, so those must be off the
    // view too -- the m_baseObject / m_minimap shape.
    CHECK_FALSE(inside(&view.baseItem()));
    CHECK(view.minimap().scene() != nullptr);
    CHECK(view.minimap().parentItem() == nullptr);
    for(const QGraphicsItem* it : view.minimap().scene()->items())
      if(it->parentItem() == nullptr)
        CHECK_FALSE(inside(it));

    // And the views still work: they are in the layout and see their scenes.
    CHECK(view.view().parentWidget() == w);
    CHECK(view.view().scene() == &view.scene());
  });
}
