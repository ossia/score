// The project-file commands have to be findable, and the two that shrink a
// project have to be distinguishable.
//
// Worth a test rather than an eyeball: the submenu is inserted above the File
// menu's last separator, because a plug-in's additions otherwise land after
// Quit. That placement depends on how another plug-in built its menu, which is
// exactly the kind of thing that breaks quietly.

#include <score_test/App.hpp>

#include <score/actions/MenuManager.hpp>

#include <QMenu>

#include <catch2/catch_test_macros.hpp>

namespace
{
//! Index of `action` among the parent menu's entries, -1 if absent.
int index_of(QMenu& parent, QAction* action)
{
  return int(parent.actions().indexOf(action));
}
}

TEST_CASE("The project-file commands are in the File menu", "[integration][menu]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto& file = ctx.menus.get().at(score::Menus::File());
    REQUIRE(file.menu() != nullptr);

    auto& project = ctx.menus.get().at(score::Menus::ProjectFiles());
    REQUIRE(project.menu() != nullptr);

    // It is a submenu of File, not a loose pile of entries.
    QAction* asAction = nullptr;
    for(auto* action : file.menu()->actions())
      if(action->menu() == project.menu())
        asAction = action;
    REQUIRE(asAction != nullptr);

    // Above the last separator, i.e. with Save/Save As rather than after Quit.
    const auto entries = file.menu()->actions();
    int lastSeparator = -1;
    for(int i = 0; i < entries.size(); i++)
      if(entries[i]->isSeparator())
        lastSeparator = i;

    REQUIRE(lastSeparator >= 0);
    CHECK(index_of(*file.menu(), asAction) < lastSeparator);

    // All five commands are there, and the two that shrink a project do not
    // read as the same command.
    QStringList labels;
    for(auto* action : project.menu()->actions())
      if(!action->isSeparator())
        labels << action->text();

    CHECK(labels.size() == 5);

    const auto has = [&](const QString& fragment) {
      for(const auto& l : labels)
        if(l.contains(fragment, Qt::CaseInsensitive))
          return true;
      return false;
    };

    CHECK(has("Consolidate"));
    CHECK(has("missing"));
    CHECK(has("unused files"));  // whole files go away
    CHECK(has("Shorten media")); // used files get shorter
    CHECK(has("Archive"));
  });
}
