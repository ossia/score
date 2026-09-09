// The comment box (Ui::TextBox) and the two things it does with a QTextDocument.
//
//  * Trailing spaces. The text is stored as HTML, and HTML collapses the
//    spaces at the end of a line: typing them moved the cursor but painted
//    nothing, and the next letter appeared where the spaces had been.
//
//  * Reflow. The document was never given a width, so it laid out at whatever
//    width its longest line wanted and the box grew sideways forever. With a
//    width it wraps to the box instead.

#include <QApplication>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextLayout>
#include <QTextLine>
#include <QTextOption>

#include <Ui/TextBox.hpp>

#include <score_test/App.hpp>

#include <catch2/catch_all.hpp>

namespace
{
//! What Ui::TextBox stores and reloads: the document's HTML.
QString roundTrip(const QString& html)
{
  QTextDocument d;
  d.setHtml(html);
  return d.toHtml();
}
}

//! Where the caret sits at the end of the first line -- what the user watches
//! move (or not) while typing.
namespace
{
double endOfLineX(QTextDocument& d)
{
  (void)d.size(); // force the layout
  auto block = d.firstBlock();
  auto* layout = block.layout();
  if(!layout || layout->lineCount() == 0)
    return 0.;
  return layout->lineAt(0).cursorToX(block.length() - 1);
}
}

TEST_CASE("a comment keeps the spaces typed at the end of a line")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTextDocument typed;
    typed.setPlainText(QStringLiteral("hello   "));

    // The comment box stores its text as HTML, where trailing spaces are the
    // first thing a naive round trip loses.
    QTextDocument reloaded;
    reloaded.setHtml(typed.toHtml());
    CHECK(reloaded.toPlainText() == QStringLiteral("hello   "));

    QTextDocument without;
    without.setPlainText(QStringLiteral("hello"));
    Ui::TextBox::setupDocument(reloaded, -1.);
    Ui::TextBox::setupDocument(without, -1.);

    // ... and the caret sits past them, so the box grows as they are typed.
    INFO("with spaces " << endOfLineX(reloaded) << " without " << endOfLineX(without));
    CHECK(endOfLineX(reloaded) > endOfLineX(without));
  });
}

TEST_CASE("the comment HTML round-trips unchanged")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTextDocument d;
    d.setPlainText(QStringLiteral("one   \ntwo"));
    const auto once = d.toHtml();
    CHECK(roundTrip(once) == once);
  });
}

TEST_CASE("a document given a width wraps instead of growing sideways")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTextDocument d;
    d.setPlainText(QStringLiteral(
        "a long line of text that has plenty of words in it to wrap around with"));

    const double natural = d.idealWidth();
    const int naturalLines = d.blockCount();
    REQUIRE(naturalLines == 1);

    d.setTextWidth(120.);
    CHECK(d.size().width() <= 121.);
    CHECK(d.size().height() > 0.);
    CHECK(d.size().width() < natural);

    // ... and giving it back its freedom restores the single line.
    d.setTextWidth(-1);
    CHECK(d.idealWidth() == Catch::Approx(natural).margin(1.));
  });
}

// What setupDocument() is for: Qt drops trailing spaces at layout time unless
// it is told not to, and a document with no width lays out as wide as it likes.
TEST_CASE("the comment document wraps only when it is given a width")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const auto text = QStringLiteral(
        "a long line of text that has plenty of words in it to wrap around with");

    QTextDocument free;
    free.setPlainText(text);
    Ui::TextBox::setupDocument(free, -1.);
    CHECK(free.textWidth() < 0.);

    QTextDocument wrapped;
    wrapped.setPlainText(text);
    Ui::TextBox::setupDocument(wrapped, 120.);
    CHECK(wrapped.textWidth() == Catch::Approx(120.));
    CHECK(wrapped.size().width() <= 121.);
    CHECK(wrapped.size().height() > free.size().height());

    // A width of zero or less means "as wide as you like", not "no room".
    Ui::TextBox::setupDocument(wrapped, 0.);
    CHECK(wrapped.textWidth() < 0.);
  });
}
