// Regression test for the headless save prompt: closing a MODIFIED document
// with applicationSettings.gui == false used to raise the raw QMessageBox in
// DocumentManager::closeDocument. Under the offscreen QPA, with no one to
// answer it, exec() aborts -- so a scripted /exit on a document the script had
// edited died in teardown with SIGABRT.
//
// score::test::close_all_documents goes through forceCloseDocument, which
// bypasses the modal, and RegressionDoubleExitTest only ever closes unmodified
// documents: nothing in the suite reached this path.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <core/application/MinimalApplication.hpp>
#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <score/command/Command.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

#include <ossia/detail/thread.hpp>

#include <QApplication>

#include <catch2/catch_test_macros.hpp>

namespace
{
const CommandGroupKey& testCommandGroup()
{
  static const CommandGroupKey k{"HeadlessCloseTest"};
  return k;
}

// Declared by hand rather than with SCORE_COMMAND_DECL: the macro's key is
// picked up by the build system's command-list scan, and nothing here needs to
// be serializable -- only to move the command stack off its saved index.
struct MarkDirty final : public score::Command
{
  void undo(const score::DocumentContext&) const override { }
  void redo(const score::DocumentContext&) const override { }
  const CommandGroupKey& parentKey() const noexcept override
  {
    return testCommandGroup();
  }
  const CommandKey& key() const noexcept override
  {
    static const CommandKey k{"MarkDirty"};
    return k;
  }
  QString description() const override { return QStringLiteral("Mark dirty"); }
  void serializeImpl(DataStreamInput&) const override { }
  void deserializeImpl(DataStreamOutput&) override { }
};
}

TEST_CASE(
    "Closing a modified document without a GUI does not ask to save",
    "[integration][regression][headless]")
{
  score::test::prepare_test_environment(/*headless=*/true);
  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

  static int argc = 1;
  static char arg0[] = "score-test";
  static char* argv[] = {arg0, nullptr};
  score::MinimalGUIApplication app{argc, argv, /*show=*/false};
  QApplication::processEvents();

  app.m_applicationSettings.gui = false;
  const auto& ctx = app.context();
  REQUIRE(ctx.applicationSettings.gui == false);

  auto* doc = score::test::new_document(ctx);
  REQUIRE(doc);

  doc->commandStack().redoAndPush(new MarkDirty);
  QApplication::processEvents();
  REQUIRE_FALSE(doc->commandStack().isAtSavedIndex());

  // Without the gui guard this reaches QMessageBox::exec() with no window
  // system able to run it, and the process aborts here instead of returning.
  CHECK(ctx.docManager.closeDocument(ctx, *doc) == true);
  QApplication::processEvents();
  CHECK(ctx.docManager.documents().empty());
}

TEST_CASE(
    "An unmodified document closes the same way with a GUI",
    "[integration][regression][headless]")
{
  // The half that proves the case above is about the flag and not about
  // closeDocument always succeeding: gui stays true, and the close still
  // returns without a prompt because the stack is at its saved index.
  score::test::prepare_test_environment(/*headless=*/true);
  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

  static int argc = 1;
  static char arg0[] = "score-test";
  static char* argv[] = {arg0, nullptr};
  score::MinimalGUIApplication app{argc, argv, /*show=*/false};
  QApplication::processEvents();

  const auto& ctx = app.context();
  REQUIRE(ctx.applicationSettings.gui == true);

  auto* doc = score::test::new_document(ctx);
  REQUIRE(doc);
  REQUIRE(doc->commandStack().isAtSavedIndex());

  CHECK(ctx.docManager.closeDocument(ctx, *doc) == true);
  QApplication::processEvents();
  CHECK(ctx.docManager.documents().empty());
}
