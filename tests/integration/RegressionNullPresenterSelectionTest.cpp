// Regression test for 724578084 — "core: guard null document presenter on
// selection change".
//
// A document's DocumentPresenter is only created when the document has a view
// (a windowed GUI). A headless application (MinimalApplication: gui settings
// on, but no window) creates documents without one, yet the selection-changed
// handler installed in Document::init() dereferenced m_presenter
// unconditionally. Any selection change — e.g. a Scenario process being
// destroyed and clearing the selection stack — crashed with a null deref.
//
// Without the fix this test dies with SIGSEGV inside the
// currentSelectionChanged lambda; with it, the presenter call is skipped.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <score/selection/Selection.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/model/IdentifiedObject.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QApplication>

#include <catch2/catch_test_macros.hpp>

TEST_CASE(
    "Selection changes in a presenter-less (headless) document do not crash",
    "[integration][regression][selection]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    // Precondition for the regression: headless documents have no presenter,
    // while applicationSettings.gui is still true so the selection handler
    // (the buggy code path) is connected.
    REQUIRE(doc->presenter() == nullptr);
    REQUIRE(ctx.applicationSettings.gui);

    auto& stack = doc->context().selectionStack;

    // Select an object: emits currentSelectionChanged -> the Document::init()
    // handler. Without the fix: m_presenter->setNewSelection on nullptr.
    Selection sel
        = Selection::fromList(QList<IdentifiedObjectAbstract*>{&doc->model()});
    stack.pushNewSelection(sel);
    QApplication::processEvents();

    CHECK(stack.currentSelection().size() == 1);

    // And clear it again: a second currentSelectionChanged emission, matching
    // the original crash scenario (a destroyed process clearing the stack).
    stack.deselect();
    QApplication::processEvents();

    CHECK(stack.currentSelection().empty());
  });
}

namespace
{
struct SelectableObject : IdentifiedObject<SelectableObject>
{
  SelectableObject(int id, QObject* parent)
      : IdentifiedObject<SelectableObject>{Id<SelectableObject>{id}, "Obj", parent}
  {
  }
};
}

// Select an object, add a monitor with a script, undo: the object stays
// selected. Pruning the monitor from the selection stack must keep the empty
// selection at the bottom of the stack, or currentSelection() becomes empty
// while the views are told the object is still selected.
TEST_CASE(
    "Removing another object keeps the selection", "[integration][regression][selection]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& stack = doc->context().selectionStack;

    QObject parent;
    auto* a = new SelectableObject{1, &parent};
    auto* b = new SelectableObject{2, &parent};
    const Selection just_a = Selection::fromList(QList<IdentifiedObjectAbstract*>{a});

    stack.pushNewSelection(just_a);
    REQUIRE(stack.currentSelection() == just_a);

    SECTION("undoing the creation of another process")
    {
      stack.pruneRecursively(b);
    }
    SECTION("another object destroyed")
    {
      stack.prune(b);
    }

    CHECK(stack.currentSelection() == just_a);
    CHECK(stack.canUnselect());

    // And it can be deselected, with the views told what was selected
    Selection old, now;
    QObject::connect(
        &stack, &score::SelectionStack::currentSelectionChanged, &parent,
        [&](const Selection& o, const Selection& n) {
      old = o;
      now = n;
    });
    stack.deselect();
    CHECK(stack.currentSelection().empty());
    CHECK(old == just_a);
    CHECK(now.empty());

    delete b;
  });
}
