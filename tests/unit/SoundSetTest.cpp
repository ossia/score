// The folder of sound files a halp::sound_set port names: listed, decoded and
// watched by the host, and published to the processor without the audio
// thread ever freeing anything.

#include <Crousti/SoundSet.hpp>

#include <score_test/App.hpp>
#include <score_test/Project.hpp>

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_all.hpp>

#include <memory>
#include <vector>

namespace
{
template <typename F>
bool spin_until(F f, int ms = 8000)
{
  QElapsedTimer t;
  t.start();
  while(!f() && t.elapsed() < ms)
    QApplication::processEvents(QEventLoop::AllEvents, 5);
  return f();
}

//! Stands for the processor: takes each snapshot as the execution command
//! would, and flags the previous one as superseded.
struct FakeProcessor
{
  std::shared_ptr<oscr::SoundSetSnapshot> current;
  std::vector<std::shared_ptr<oscr::SoundSetSnapshot>> disposed;
  int published{};

  oscr::SoundSetLoader::publish_function publish()
  {
    return [this](std::shared_ptr<oscr::SoundSetSnapshot> next,
                  oscr::SoundSetSnapshot* previous) {
      current = std::move(next);
      if(previous)
        previous->superseded = true;
      published++;
    };
  }
  oscr::SoundSetLoader::dispose_function dispose()
  {
    return [this](std::vector<std::shared_ptr<oscr::SoundSetSnapshot>> s) {
      for(auto& e : s)
        disposed.push_back(std::move(e));
    };
  }

  bool all(halp::sound_status st) const
  {
    if(!current || current->views.empty())
      return false;
    for(auto& v : current->views)
      if(v.status != st && v.status != halp::sound_status::failed)
        return false;
    return true;
  }
  std::vector<std::string> names() const
  {
    std::vector<std::string> res;
    if(current)
      for(auto& v : current->views)
        res.emplace_back(v.name);
    return res;
  }
};

oscr::SoundSetOptions options(halp::sound_decoding d = halp::sound_decoding::all)
{
  oscr::SoundSetOptions o;
  o.decoding = d;
  o.rate = 48000;
  return o;
}
}

TEST_CASE("a sound set lists a folder's sound files in natural order and decodes them")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    score::test::write_wav(dir.path() + "/b10.wav", 0.1);
    score::test::write_wav(dir.path() + "/b2.wav", 0.1);
    score::test::write_wav(dir.path() + "/A1.wav", 0.1);
    QFile{dir.path() + "/notes.txt"}.open(QIODevice::WriteOnly);
    QDir{dir.path()}.mkdir("sub");
    score::test::write_wav(dir.path() + "/sub/c.wav", 0.1);

    FakeProcessor proc;
    oscr::SoundSetLoader loader{options(), proc.publish(), proc.dispose(), nullptr};
    loader.setPath(dir.path());

    REQUIRE(spin_until([&] { return proc.all(halp::sound_status::ready); }));
    CHECK(proc.names() == std::vector<std::string>{"A1.wav", "b2.wav", "b10.wav"});
    for(auto& v : proc.current->views)
    {
      CHECK(v.channels >= 1);
      CHECK(v.frames > 0);
      CHECK(v.rate == 48000);
      CHECK(v.data != nullptr);
    }
  });
}

TEST_CASE("a sound set keeps the files it cannot decode, as failed")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    score::test::write_wav(dir.path() + "/1.wav", 0.1);
    {
      QFile f{dir.path() + "/2.wav"};
      REQUIRE(f.open(QIODevice::WriteOnly));
      f.write("this is not a sound");
    }
    score::test::write_wav(dir.path() + "/3.wav", 0.1);

    FakeProcessor proc;
    oscr::SoundSetLoader loader{options(), proc.publish(), proc.dispose(), nullptr};
    loader.setPath(dir.path());

    REQUIRE(spin_until([&] { return proc.all(halp::sound_status::ready); }));
    REQUIRE(proc.current->views.size() == 3);
    CHECK(proc.current->views[0].status == halp::sound_status::ready);
    CHECK(proc.current->views[1].status == halp::sound_status::failed);
    CHECK(proc.current->views[2].status == halp::sound_status::ready);
  });
}

TEST_CASE("a sound set picks up new files and keeps the unchanged ones as they are")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    score::test::write_wav(dir.path() + "/a.wav", 0.1);

    FakeProcessor proc;
    oscr::SoundSetLoader loader{options(), proc.publish(), proc.dispose(), nullptr};
    loader.setPath(dir.path());
    REQUIRE(spin_until([&] { return proc.all(halp::sound_status::ready); }));
    const auto* a_data = proc.current->views[0].data;

    score::test::write_wav(dir.path() + "/b.wav", 0.1);
    REQUIRE(spin_until([&] {
      return proc.names().size() == 2 && proc.all(halp::sound_status::ready);
    }));
    CHECK(proc.names() == std::vector<std::string>{"a.wav", "b.wav"});
    // Not decoded again
    CHECK(proc.current->views[0].data == a_data);
  });
}

TEST_CASE("a removed sound lives on while it is pinned, and is freed after")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    score::test::write_wav(dir.path() + "/a.wav", 0.1);
    score::test::write_wav(dir.path() + "/b.wav", 0.1);

    FakeProcessor proc;
    oscr::SoundSetLoader loader{options(), proc.publish(), proc.dispose(), nullptr};
    loader.setPath(dir.path());
    REQUIRE(spin_until([&] { return proc.all(halp::sound_status::ready); }));

    // A voice plays b.wav
    std::weak_ptr<oscr::SoundSetEntry> b = proc.current->entries[1];
    halp::sound_pin pin{proc.current->views[1]};

    QFile::remove(dir.path() + "/b.wav");
    REQUIRE(spin_until([&] { return proc.names().size() == 1; }));

    loader.reap();
    CHECK(!b.expired());

    pin.reset();
    REQUIRE(spin_until([&] {
      loader.reap();
      return b.expired();
    }));
  });
}

TEST_CASE("an on-demand sound set decodes the files that are requested")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    score::test::write_wav(dir.path() + "/a.wav", 0.1);
    score::test::write_wav(dir.path() + "/b.wav", 0.1);

    FakeProcessor proc;
    oscr::SoundSetLoader loader{
        options(halp::sound_decoding::on_demand), proc.publish(), proc.dispose(),
        nullptr};
    loader.setPath(dir.path());
    REQUIRE(spin_until([&] { return proc.all(halp::sound_status::unloaded); }));

    proc.current->views[1].requested->store(true);
    REQUIRE(spin_until([&] {
      return proc.current->views[1].status == halp::sound_status::ready;
    }));
    CHECK(proc.current->views[0].status == halp::sound_status::unloaded);
  });
}

TEST_CASE("a sound set hands what the processor may still read over when it goes")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    score::test::write_wav(dir.path() + "/a.wav", 0.1);

    FakeProcessor proc;
    {
      oscr::SoundSetLoader loader{options(), proc.publish(), proc.dispose(), nullptr};
      loader.setPath(dir.path());
      REQUIRE(spin_until([&] { return proc.all(halp::sound_status::ready); }));
    }
    CHECK(!proc.disposed.empty());
  });
}
