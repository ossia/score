// A JSFX script finds what it imports and the files it lists under the
// Effects and Data folders of its own tree, whichever scripts were loaded
// before it.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Process/Dataflow/WidgetInlets.hpp>

#include <QDir>
#include <QTemporaryDir>

#include <YSFX/ProcessModel.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
const QString ysfx_uuid = QStringLiteral("cb6b50d3-82c0-4356-a282-317da8b8022f");

// A file slider lists the files of Data/sounds
constexpr auto script = R"(desc:roots test
slider1:/sounds:a.wav:Sound
out_pin:out
@sample
spl0 = 0;
)";

//! <root>/Effects/roots.jsfx and <root>/Data/sounds/<sounds>
QString make_tree(const QString& root, const QStringList& sounds)
{
  const QString script_path = root + "/Effects/roots.jsfx";
  score::test::write_file(script_path, script);
  for(const auto& s : sounds)
    score::test::write_file(root + "/Data/sounds/" + s, "RIFF");
  return script_path;
}

int choices(YSFX::ProcessModel& p)
{
  auto* c = qobject_cast<Process::ComboBox*>(p.inlet(Id<Process::Port>(4)));
  REQUIRE(c);
  return int(c->alternatives.size());
}
}

TEST_CASE(
    "a JSFX script lists the files of its own Data folder", "[integration][ysfx]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString first = make_tree(dir.filePath("first"), {"a.wav", "b.wav", "c.wav"});
    const QString second = make_tree(dir.filePath("second"), {"a.wav"});

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* p1 = static_cast<YSFX::ProcessModel*>(
        score::test::add_process(*doc, ysfx_uuid, first));
    if(!p1 || !p1->fx)
      SKIP("YSFX not built");
    auto* p2 = static_cast<YSFX::ProcessModel*>(
        score::test::add_process(*doc, ysfx_uuid, second));
    REQUIRE(p2);
    REQUIRE(p2->fx);

    CHECK(choices(*p1) == 3);
    CHECK(choices(*p2) == 1);
  });
}
