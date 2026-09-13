// The skin editor is a sub-tab of "User interface", inlined rather than a
// dialog. Also pins the style combo: it used to show whichever style sorted
// first, misreporting a Regular font as Bold and writing that back.

#include <score_test/App.hpp>

#include <Scenario/Settings/ScenarioSettingsView.hpp>
#include <Scenario/Settings/SkinEditorWidget.hpp>

#include <score/model/Skin.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>

#include <QComboBox>
#include <QFontDatabase>
#include <QTabWidget>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("The skin editor is a sub-tab of the interface settings", "[integration][skin]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::registerApplicationFonts();

    Scenario::Settings::View v;
    // getWidget() is private on the override; go through the base.
    score::GlobalSettingsView& base = v;
    auto* w = base.getWidget();
    REQUIRE(w != nullptr);

    auto* tabs = qobject_cast<QTabWidget*>(w);
    REQUIRE(tabs != nullptr);
    REQUIRE(tabs->count() == 2);

    QStringList names;
    for(int i = 0; i < tabs->count(); i++)
      names << tabs->tabText(i);
    INFO("tabs: " << names.join(", ").toStdString());
    CHECK(names.contains("Interface"));
    CHECK(names.contains("Skin"));

    // Inlined, not a dialog.
    const int skinTab = names.indexOf("Skin");
    CHECK(
        qobject_cast<Scenario::Settings::SkinEditorWidget*>(tabs->widget(skinTab))
        != nullptr);
  });
}

TEST_CASE("The skin combo shows the loaded skin", "[integration][skin]")
{
  // The presenter pushes the current skin at construction, but the editor is
  // built lazily when the page is first shown, so the value used to be
  // dropped and the combo stayed on the first entry whatever was loaded.
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::registerApplicationFonts();

    Scenario::Settings::View v;
    // Before the widget exists, exactly as the presenter does it.
    v.setSkin(QStringLiteral(":/skin/GalmuriMicroSkin.json"));

    score::GlobalSettingsView& base = v;
    auto* tabs = qobject_cast<QTabWidget*>(base.getWidget());
    REQUIRE(tabs != nullptr);

    Scenario::Settings::SkinEditorWidget* ed = nullptr;
    for(int i = 0; i < tabs->count(); i++)
      if(auto* e = qobject_cast<Scenario::Settings::SkinEditorWidget*>(tabs->widget(i)))
        ed = e;
    REQUIRE(ed != nullptr);

    // The skin combo is the one holding skin paths.
    QComboBox* skinCombo = nullptr;
    for(auto* c : ed->findChildren<QComboBox*>())
      if(c->findData(QStringLiteral(":/skin/DefaultSkin.json")) != -1)
        skinCombo = c;
    REQUIRE(skinCombo != nullptr);

    INFO("combo shows: " << skinCombo->currentText().toStdString());
    CHECK(skinCombo->currentData().toString() == ":/skin/GalmuriMicroSkin.json");

    // The setting's default is the bare word "Default", not a path.
    ed->setSkin(QStringLiteral("Default"));
    CHECK(skinCombo->currentData().toString() == ":/skin/DefaultSkin.json");
  });
}

TEST_CASE("The skin editor reports the font's real style", "[integration][skin]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::registerApplicationFonts();

    // Galmuri11 ships Regular, Bold and Condensed, and "Bold" sorts before
    // "Regular", so a Regular font is exactly the case that used to be shown
    // wrong.
    const QStringList styles = QFontDatabase::styles(QStringLiteral("Galmuri11"));
    REQUIRE(styles.contains("Bold"));
    REQUIRE(styles.contains("Regular"));
    INFO("Galmuri11 styles in order: " << styles.join(", ").toStdString());

    auto& skin = score::Skin::instance();
    auto roles = skin.fonts();
    REQUIRE(!roles.empty());

    // Put a plain Regular Galmuri11 on the first role.
    QFont* f = roles.front().second;
    *f = QFont{QStringLiteral("Galmuri11")};
    f->setPixelSize(12);
    f->setStyleName(QString{});

    Scenario::Settings::SkinEditorWidget ed;
    const auto combos = ed.findChildren<QComboBox*>();
    REQUIRE(combos.size() >= 3); // skin, family, style

    // The style combo is the one populated with this family's styles.
    QComboBox* styleCombo = nullptr;
    for(auto* c : combos)
      if(c->count() == styles.size() && c->findText(QStringLiteral("Regular")) != -1)
        styleCombo = c;
    REQUIRE(styleCombo != nullptr);

    INFO("style combo shows: " << styleCombo->currentText().toStdString());
    CHECK(styleCombo->currentText() == "Regular");
  });
}
