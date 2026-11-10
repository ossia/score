// A JSFX effect added, edited and removed while the document plays, in every
// way score_test/LivePlugin.hpp knows: the effect, written by the test, has a
// stereo signal path, a slider and a MIDI output where it sends a note.

#include <score_test/App.hpp>
#include <score_test/LivePlugin.hpp>

#include <YSFX/ProcessMetadata.hpp>
#include <YSFX/ProcessModel.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace
{
constexpr auto effect = R"(desc:Score Test Live Gain
slider1:0.5<0,1,0.01>Gain
in_pin:left input
in_pin:right input
out_pin:left output
out_pin:right output

@init
sent = 0;

@slider
gain = slider1;

@block
sent == 0 ? (
  midisend(0, 0x90, 60 | (100 * 256));
  sent = 1;
);

@sample
spl0 *= gain;
spl1 *= gain;
)";

QString write_effect(const QTemporaryDir& dir)
{
  const QString path = dir.filePath(QStringLiteral("live_gain.jsfx"));
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(effect);
  return path;
}
}

TEST_CASE(
    "a JSFX effect added, edited and removed while playing, in every way",
    "[integration][execution][ysfx]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString path = write_effect(dir);

  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    score::test::live::options opt;
    opt.what = {Metadata<ConcreteKey_k, YSFX::ProcessModel>::get(), {}, path};
    opt.files = {QUrl::fromLocalFile(path)};

    std::vector<std::string> failures;
    int ok = 0;
    score::test::live::recorder rec;
    rec.result
        = [&](std::string_view phase, std::string_view verdict, std::string_view detail) {
      if(verdict == "fail")
        failures.push_back(std::string{phase} + ": " + std::string{detail});
      else if(verdict == "ok")
        ok++;
    };
    score::test::live::run(ctx, opt, rec);
    for(auto& f : failures)
      UNSCOPED_INFO(f);
    CHECK(failures.empty());
    CHECK(ok > 0);
  });
}
