// A widget that copies qApp's palette to tint itself pins every role it
// copied and stops following later skins. Only the tinted roles may resolve.

#include <score_test/App.hpp>

#include <score/model/Skin.hpp>
#include <score/widgets/ValidationPalette.hpp>

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QWidget>

#include <catch2/catch_test_macros.hpp>

namespace
{
int setRoles(const QPalette& p)
{
  int n = 0;
  for(int r = 0; r < QPalette::NColorRoles; r++)
    if(p.isBrushSet(QPalette::Active, QPalette::ColorRole(r)))
      n++;
  return n;
}
}

TEST_CASE("A validity tint pins only the roles it sets", "[integration][skin]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QWidget w;

    score::setInputValidity(w, score::InputValidity::Invalid);
    CHECK(setRoles(w.palette()) == 3);
    CHECK(w.palette().isBrushSet(QPalette::Active, QPalette::Base));

    // Back to valid: nothing pinned, so the widget inherits again.
    score::setInputValidity(w, score::InputValidity::Valid);
    CHECK(setRoles(w.palette()) == 0);
  });
}

TEST_CASE("A tinted widget follows the skin", "[integration][skin]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto& skin = score::Skin::instance();
    auto read = [](const char* path) {
      QFile f{QString::fromUtf8(path)};
      REQUIRE(f.open(QFile::ReadOnly));
      return QJsonDocument::fromJson(f.readAll()).object();
    };
    skin.load(read(":/skin/DefaultSkin.json"));

    QWidget w;
    score::setInputValidity(w, score::InputValidity::Invalid);
    const QColor before = w.palette().color(QPalette::Base);
    REQUIRE(before == skin.Warn3.darker.brush.color());

    skin.load(read(":/skin/NordSkin.json"));

    CHECK(w.palette().color(QPalette::Base) != before);
    CHECK(w.palette().color(QPalette::Base) == skin.Warn3.darker.brush.color());
  });
}

TEST_CASE("An address fragment field tints on bad input", "[integration][skin]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    State::AddressFragmentLineEdit e{nullptr};

    e.setText(QStringLiteral("valid_name"));
    CHECK(setRoles(e.palette()) == 0);

    e.setText(QStringLiteral("bad/name"));
    CHECK(setRoles(e.palette()) == 3);
  });
}
