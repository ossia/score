// Threedim/CalibrationFiles: one reader and writer of name.json and its maps,
// with the "files" semantics of the multi-window outputs.

#include <Gfx/Window/CalibrationSet.hpp>
#include <Threedim/CalibrationFiles.hpp>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

using namespace Threedim;

namespace
{
score::gfx::FloatImage map(int w, int h, int channels, float base)
{
  score::gfx::FloatImage img;
  img.width = w;
  img.height = h;
  img.channels = channels;
  img.data.resize(std::size_t(w) * h * channels);
  for(std::size_t i = 0; i < img.data.size(); i++)
    img.data[i] = base + float(i);
  if(channels == 4)
    for(std::size_t i = 3; i < img.data.size(); i += 4)
      img.data[i] = (i / 4) % 2 ? 1.f : 0.f;
  return img;
}

QStringList listed(const QString& json)
{
  QStringList out;
  for(const auto& v : readJsonFile(json).value("files").toArray())
    out.push_back(v.toString());
  return out;
}

void writeRaw(const QString& path, const QByteArray& b)
{
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(b);
}
}

TEST_CASE("a calibration write lists the maps it wrote", "[threedim][calibration][files]")
{
  QTemporaryDir dir;
  const QString base = dir.filePath("room_p1");
  CalibrationWrite w;
  w.base = base;
  w.maps.push_back({".xyz.pfm", map(4, 3, 4, 0.f)});
  w.maps.push_back({".lum.pfm", map(4, 3, 3, 10.f)});
  w.fields["mode"] = "depth";
  QString err;
  REQUIRE(writeCalibration(w, &err));
  CHECK(
      listed(base + ".json")
      == QStringList{"room_p1.xyz.pfm", "room_p1.valid.pfm", "room_p1.lum.pfm"});
  const auto o = readJsonFile(base + ".json");
  CHECK(o["width"].toInt() == 4);
  CHECK(o["height"].toInt() == 3);
  CHECK(o["mode"].toString() == "depth");
  CHECK(Gfx::calibrationListing(base) == listed(base + ".json"));

  const auto c = loadCalibration(base + ".json");
  INFO(c.error.toStdString());
  REQUIRE(c.ok());
  CHECK(c.xyz == map(4, 3, 4, 0.f).data);
  CHECK(c.lum == map(4, 3, 3, 10.f).data);
}

TEST_CASE("a calibration write merges, refuses, never clobbers", "[threedim][calibration][files]")
{
  QTemporaryDir dir;
  const QString base = dir.filePath("p");
  CalibrationWrite first;
  first.base = base;
  first.maps = {{".xyz.pfm", map(4, 3, 4, 0.f)}, {".lum.pfm", map(4, 3, 3, 1.f)}};
  first.fields["intrinsics"] = QJsonArray{1000, 1000, 2, 1.5};
  first.fields["note"] = "kept";
  REQUIRE(writeCalibration(first));

  SECTION("create only")
  {
    CalibrationWrite again = first;
    again.createOnly = true;
    QString err;
    CHECK_FALSE(writeCalibration(again, &err));
    CHECK(err.contains("exists"));
  }
  SECTION("another projector size")
  {
    CalibrationWrite bigger;
    bigger.base = base;
    bigger.maps = {{".xyz.pfm", map(8, 6, 4, 5.f)}};
    QString err;
    CHECK_FALSE(writeCalibration(bigger, &err));
    CHECK(err.contains("4 x 3"));
    CHECK(loadCalibration(base).xyz == map(4, 3, 4, 0.f).data);
  }
  SECTION("merge keeps the keys, and only the files it is told to")
  {
    CalibrationWrite geometry;
    geometry.base = base;
    geometry.maps = {{".xyz.pfm", map(4, 3, 4, 7.f)}};
    geometry.fields["rms"] = 0.5;
    REQUIRE(writeCalibration(geometry));
    const auto o = readJsonFile(base + ".json");
    CHECK(o["note"].toString() == "kept");
    CHECK(o["intrinsics"].toArray().size() == 4);
    CHECK(o["rms"].toDouble() == 0.5);
    CHECK(listed(base + ".json") == QStringList{"p.xyz.pfm", "p.valid.pfm"});
    CHECK(loadCalibration(base).lum.empty());

    geometry.keepListed = {".lum.pfm"};
    REQUIRE(writeCalibration(first));
    REQUIRE(writeCalibration(geometry));
    CHECK(listed(base + ".json") == QStringList{"p.xyz.pfm", "p.valid.pfm", "p.lum.pfm"});
  }
  SECTION("name.json is replaced as a whole")
  {
    QByteArray before;
    {
      QFile f{base + ".json"};
      REQUIRE(f.open(QIODevice::ReadOnly));
      before = f.readAll();
    }
    QFile reader{base + ".json"};
    REQUIRE(reader.open(QIODevice::ReadOnly));
    CalibrationWrite note;
    note.base = base;
    note.fields["note"] = "changed";
    REQUIRE(writeCalibration(note));
    CHECK(reader.readAll() == before);
    CHECK(readJsonFile(base + ".json")["note"].toString() == "changed");
  }
}

TEST_CASE("a calibration reads only the files it lists", "[threedim][calibration][files]")
{
  QTemporaryDir dir;
  const QString base = dir.filePath("p");
  REQUIRE(score::gfx::writeFloatImageSplit(base, map(4, 3, 4, 0.f)));
  REQUIRE(score::gfx::writePFM(base + ".lum.pfm", map(4, 3, 3, 1.f)));

  SECTION("no name.json: the files on disk")
  {
    const auto c = loadCalibration(base + ".xyz.pfm");
    REQUIRE(c.ok());
    CHECK(c.lum == map(4, 3, 3, 1.f).data);
    CHECK_FALSE(c.hasPose);
  }
  SECTION("a stale lum.pfm the json does not list")
  {
    writeRaw(
        base + ".json",
        R"({"files": ["p.xyz.pfm", "p.valid.pfm"], "width": 4, "height": 3})");
    const auto c = loadCalibration(base);
    REQUIRE(c.ok());
    CHECK(c.lum.empty());
  }
  SECTION("a stale xyz.pfm the json does not list")
  {
    writeRaw(base + ".json", R"({"files": ["p.lum.pfm"]})");
    const auto c = loadCalibration(base);
    CHECK_FALSE(c.ok());
    CHECK(c.error.contains("no XYZ map"));
  }
  SECTION("an XYZ map of another size than name.json says")
  {
    writeRaw(base + ".json", R"({"files": ["p.xyz.pfm"], "width": 8, "height": 3})");
    const auto c = loadCalibration(base);
    CHECK_FALSE(c.ok());
  }
  SECTION("the listed names, relative to name.json")
  {
    QDir{dir.path()}.mkdir("maps");
    REQUIRE(score::gfx::writeFloatImageSplit(dir.filePath("maps/other"), map(4, 3, 4, 3.f)));
    writeRaw(
        base + ".json",
        R"({"files": ["maps/other.xyz.pfm", "maps/other.valid.pfm"],
            "intrinsics": [100, 101, 2, 1], "mode": "viewer",
            "worldFromProjector": [1,0,0,0, 0,1,0,0, 0,0,1,0, 5,6,7,1]})");
    const auto c = loadCalibration(base);
    REQUIRE(c.ok());
    CHECK(c.xyz == map(4, 3, 4, 3.f).data);
    CHECK(c.hasModel());
    CHECK(c.viewerMode);
    CHECK(c.pose[12] == 5.f);
    CHECK(c.intrinsics[1] == 101.f);
  }
}

TEST_CASE("a saved calibration keeps its json and lists its maps", "[threedim][calibration][files]")
{
  QTemporaryDir dir;
  CalibrationFile c;
  c.base = dir.filePath("room_p2");
  c.width = 4;
  c.height = 3;
  c.xyz = map(4, 3, 4, 0.f).data;
  c.black = map(4, 3, 3, 0.5f).data;
  c.intrinsics = {100, 100, 1.5, 1};
  c.hasPose = true;
  c.pose[13] = 2.f;
  c.json["sequence"] = "gray";
  REQUIRE(saveCalibration(c));
  const auto r = loadCalibration(c.base);
  REQUIRE(r.ok());
  CHECK(r.xyz == c.xyz);
  CHECK(r.black == c.black);
  CHECK(r.pose == c.pose);
  CHECK(r.json["sequence"].toString() == "gray");
  CHECK(r.json["mode"].toString() == "depth");
}

TEST_CASE("a renamed calibration stays in its set", "[threedim][calibration][files]")
{
  CHECK(calibrationSetRenamed("/a/room_p2", "_reg") == "/a/room_reg_p2");
  CHECK(calibrationSetRenamed("/a/room", "_reg") == "/a/room_reg");
  CHECK(Gfx::calibrationSetBase(calibrationSetRenamed("/a/room_p2", "_reg")) == "/a/room_reg");
  CHECK(Gfx::calibrationSetName("/a/room_reg", 2) == calibrationSetRenamed("/a/room_p2", "_reg"));
}

TEST_CASE("number lists refuse what is not a finite number", "[threedim][calibration][files]")
{
  CHECK(parseNumbers("1, 2.5;-3  4e1") == std::vector<float>{1.f, 2.5f, -3.f, 40.f});
  CHECK(parseNumbers("") == std::vector<float>{});
  CHECK_FALSE(parseNumbers("1 nan 2"));
  CHECK_FALSE(parseNumbers("1 inf"));
  CHECK_FALSE(parseNumbers("1 2x"));
  CHECK(splitPaths(" a.json ;b.json|\n c d.json \n") == std::vector<std::string>{"a.json", "b.json", "c d.json"});
}
