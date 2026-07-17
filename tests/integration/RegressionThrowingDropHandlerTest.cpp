// Regression test for e9bcc5fdc — "process: don't let a throwing drop handler
// escape the drop API".
//
// Some ProcessDropHandler::dropCustom overrides run parsers that can throw on
// malformed input, but the public getCustomDrops API is noexcept: before the
// fix dropCustom was noexcept too, so a throwing handler called
// std::terminate and tore down the editor. The fix drops noexcept from the
// dropCustom contract and catches everything in getCustomDrops.
//
// Without the fix this test dies via std::terminate; with it, getCustomDrops
// swallows both std::exception and non-std exceptions and produces no drops.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Drop/ProcessDropHandler.hpp>

#include <core/document/Document.hpp>

#include <QMimeData>

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

namespace
{
class ThrowingDropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("9f2c4be2-4c6a-4db1-93a4-1a5e6f2b7c01")

  void dropCustom(
      std::vector<ProcessDrop>& drops, const QMimeData& mime,
      const score::DocumentContext& ctx) const override
  {
    throw std::runtime_error("malformed input");
  }
};

class ThrowingIntDropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("c0a7f6a4-2f6e-49d6-8b3a-52706e6a9b02")

  void dropCustom(
      std::vector<ProcessDrop>& drops, const QMimeData& mime,
      const score::DocumentContext& ctx) const override
  {
    throw 42; // a non-std::exception payload: the catch(...) branch
  }
};
}

TEST_CASE(
    "A throwing dropCustom handler is contained by getCustomDrops",
    "[integration][regression][drop]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    QMimeData mime;
    mime.setText(QStringLiteral("not a valid payload"));

    std::vector<Process::ProcessDropHandler::ProcessDrop> drops;

    // std::exception path. Without the fix: std::terminate.
    ThrowingDropHandler handler;
    handler.getCustomDrops(drops, mime, doc->context());
    CHECK(drops.empty());

    // catch(...) path.
    ThrowingIntDropHandler int_handler;
    int_handler.getCustomDrops(drops, mime, doc->context());
    CHECK(drops.empty());
  });
}
