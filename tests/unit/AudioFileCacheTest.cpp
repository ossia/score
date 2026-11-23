// The sound files score has loaded are shared (AudioFileManager): asking for
// the same path again gives the same file. A recording can write a new take
// to a path that was already loaded: the next request for it loads the new
// content, and invalidate() forgets a path at once.

#include <Media/MediaFileHandle.hpp>

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

namespace
{
void u32(QByteArray& o, quint32 v)
{
  for(int i = 0; i < 4; i++)
    o.append(char((v >> (8 * i)) & 0xff));
}
void u16(QByteArray& o, quint16 v)
{
  for(int i = 0; i < 2; i++)
    o.append(char((v >> (8 * i)) & 0xff));
}

//! 16-bit mono silence of `frames` frames.
void write_wav(const QString& path, int frames)
{
  const quint32 data_size = quint32(frames * 2);
  QByteArray out;
  out.append("RIFF");
  u32(out, 36 + data_size);
  out.append("WAVE");
  out.append("fmt ");
  u32(out, 16);
  u16(out, 1);
  u16(out, 1);
  u32(out, 44100);
  u32(out, 44100 * 2);
  u16(out, 2);
  u16(out, 16);
  out.append("data");
  u32(out, data_size);
  out.append(QByteArray(data_size, '\0'));
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
  REQUIRE(f.write(out) == out.size());
}
}

TEST_CASE("A sound file rewritten on disk is loaded again", "[media][cache]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("take.wav");
    auto& mgr = Media::AudioFileManager::instance();

    write_wav(path, 1000);
    auto a = mgr.get(path, -1);
    REQUIRE(a);
    CHECK(a->samples() == 1000);
    CHECK(mgr.get(path, -1) == a);

    // A new take recorded to the same path
    write_wav(path, 3000);
    {
      QFile f{path};
      REQUIRE(f.open(QIODevice::ReadWrite));
      REQUIRE(f.setFileTime(
          QDateTime::currentDateTime().addSecs(5), QFileDevice::FileModificationTime));
    }
    auto b = mgr.get(path, -1);
    REQUIRE(b);
    CHECK(b != a);
    CHECK(b->samples() == 3000);
    // What was given out keeps what it had
    CHECK(a->samples() == 1000);
    CHECK(mgr.get(path, -1) == b);

    // Forgotten on request
    mgr.invalidate(path);
    auto c = mgr.get(path, -1);
    REQUIRE(c);
    CHECK(c != b);
    CHECK(c->samples() == 3000);
  });
}
