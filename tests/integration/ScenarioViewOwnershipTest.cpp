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
// The commit deliberately did NOT fix the same shape for the by-value QWidget
// members that a layout reparents (m_view, m_timeRulerView, m_minimapView,
// m_minimap) -- that is a scenario-UI refactor left for its own change. They
// are pinned expected-red in their own case below so the remainder cannot be
// forgotten silently: the tag comes off when that refactor lands.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>

#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>

#include <QGraphicsScene>
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

// DEFECT REMAINDER, pinned expected-red (2026-09-01), from 6227fbd07f's own
// commit message: ProcessGraphicsView m_view, TimeRulerGraphicsView
// m_timeRulerView, MinimapGraphicsView m_minimapView and Minimap m_minimap are
// by-value QWidget members that end up in a layout, which reparents them into
// the widget Qt deletes. Fixing them means heap-allocating and touching ~26
// call sites, deferred to a scenario-UI refactor. Until then the widget MUST
// NOT be deleted before the view (the app-side teardown order), and this pin
// is the reminder: it goes green -- and the tag comes off -- when no QWidget
// direct child of the view's widget lives at a by-value member address.
TEST_CASE(
    "the remaining by-value widget members are still Qt-owned",
    "[integration][scenario][ownership][gui][!shouldfail]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& view = viewOf(*doc);

    // m_view is a by-value member; the layout made it a descendant of the
    // widget. The CORRECT state (post-refactor) is that the graphics view Qt
    // owns is not at the by-value member's address.
    const auto* qtOwned = view.getWidget()->findChild<QGraphicsView*>();
    CHECK(static_cast<const void*>(qtOwned) != static_cast<const void*>(&view.view()));
  });
}
