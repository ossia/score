// The program cache was keyed on the main source and its directory only, so
// editing a file it #includes left the cached program as it was until score
// restarted (shader board U52). A cached program whose includes changed on
// disk is rebuilt.
#include <Gfx/ShaderProgram.hpp>

#include <score_test/App.hpp>

#include <catch2/catch_test_macros.hpp>

#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>

namespace
{
void write(const QString& path, const QByteArray& text, const QDateTime& when)
{
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
  f.write(text);
  f.close();
  // Explicit times: the edit must not depend on the clock's granularity.
  REQUIRE(f.open(QIODevice::ReadWrite));
  REQUIRE(f.setFileTime(when, QFileDevice::FileModificationTime));
}
}

TEST_CASE("editing an included file rebuilds the cached program", "[gfx][include][U52]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString header = dir.path() + "/tint.glsl";
  const QString shader = dir.path() + "/tinted.fs";
  const QDateTime t0 = QDateTime::currentDateTime().addSecs(-60);
  write(header, "vec4 tint() { return vec4(0.125); }\n", t0);
  const QByteArray fs = R"_(/*{ "ISFVSN": "2.0", "INPUTS": [] }*/
#include "tint.glsl"
void main() { gl_FragColor = tint(); }
)_";
  write(shader, fs, t0);

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    const auto source = Gfx::programFromISFFragmentShaderPath(shader, fs);
    auto& cache = Gfx::ProgramCache::instance();

    auto [first, err1] = cache.get(source, shader);
    INFO(err1.toStdString());
    REQUIRE(first);
    CHECK(first->fragment.contains("0.125"));

    write(header, "vec4 tint() { return vec4(0.875); }\n", t0.addSecs(30));
    auto [second, err2] = cache.get(source, shader);
    INFO(err2.toStdString());
    REQUIRE(second);
    CHECK(second->fragment.contains("0.875"));
    CHECK_FALSE(second->fragment.contains("0.125"));

    // Unchanged includes: the cached program is reused as before.
    auto [third, err3] = cache.get(source, shader);
    REQUIRE(third);
    CHECK(third->fragment == second->fragment);
  });
}
