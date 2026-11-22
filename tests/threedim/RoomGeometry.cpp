#include <Threedim/RoomGeometry.hpp>

#include <QFile>
#include <QQuaternion>
#include <QVector3D>
#include <QTemporaryDir>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <random>

using Catch::Approx;
using namespace Threedim;

TEST_CASE("Room box: rays from inside meet the right wall", "[threedim][room]")
{
  const auto room = RoomGeometry::box(2.f, 3.f, 1.5f, 1.5f, 1.2f, 2.f);
  CHECK(room.triangleCount() == 12);
  CHECK(room.cast({0, 0, 0}, {0, 0, -1}) == Approx(2.f));
  CHECK(room.cast({0, 0, 0}, {0, 0, 1}) == Approx(3.f));
  CHECK(room.cast({0, 0, 0}, {-1, 0, 0}) == Approx(1.5f));
  CHECK(room.cast({0, 0, 0}, {0, -1, 0}) == Approx(1.2f));
  const float s = std::sqrt(0.5f);
  CHECK(room.cast({0, 0, 0}, {s, 0, -s}) == Approx(1.5f / s));
  CHECK(room.cast({0.5f, 0, 0}, {0, 0, -1}) == Approx(2.f));
}

TEST_CASE("Room box: an open side is not hit", "[threedim][room]")
{
  const auto room = RoomGeometry::box(2.f, 0.f, 1.f, 1.f, 1.f, 0.f);
  CHECK(room.triangleCount() == 8);
  CHECK(room.cast({0, 0, 0}, {0, 0, 1}) < 0.f);
  CHECK(room.cast({0, 0, 0}, {0, 1, 0}) < 0.f);
}

TEST_CASE("Room geometry: the hierarchy finds what a brute force does", "[threedim][room]")
{
  RoomGeometry room;
  std::mt19937 rng{3};
  std::uniform_real_distribution<float> u(-5.f, 5.f);
  std::vector<std::array<float, 9>> tris;
  for(int i = 0; i < 600; i++)
  {
    std::array<float, 9> t;
    const float cx = u(rng), cy = u(rng), cz = u(rng);
    for(int k = 0; k < 9; k++)
      t[k] = (k % 3 == 0 ? cx : k % 3 == 1 ? cy : cz) + u(rng) * 0.1f;
    tris.push_back(t);
    room.addTriangle(t);
  }
  room.build();
  int agree = 0, n = 0, hits = 0;
  for(int i = 0; i < 300; i++)
  {
    std::array<float, 3> d{u(rng), u(rng), u(rng)};
    const float l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    for(auto& c : d)
      c /= l;
    float best = -1.f;
    for(const auto& t : tris)
    {
      RoomGeometry g;
      g.addTriangle(t);
      g.build();
      if(const float h = g.cast({0, 0, 0}, d); h > 0.f && (best < 0.f || h < best))
        best = h;
    }
    const float got = room.cast({0, 0, 0}, d);
    n++;
    hits += best > 0.f;
    agree += (best < 0.f && got < 0.f) || std::abs(best - got) < 1e-4f;
  }
  CHECK(agree == n);
  CHECK(hits > 30);
}

TEST_CASE("Room geometry: OBJ and PLY meshes load", "[threedim][room]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  {
    QFile f{dir.filePath("wall.obj")};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write("# a wall\nv -1 -1 -2\nv 1 -1 -2\nv 1 1 -2\nv -1 1 -2\nf 1/1 2/2 3/3 4/4\n");
  }
  {
    QFile f{dir.filePath("wall.ply")};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write("ply\nformat ascii 1.0\nelement vertex 4\nproperty float x\nproperty float y\n"
            "property float z\nelement face 1\nproperty list uchar int vertex_indices\n"
            "end_header\n-1 -1 -3\n1 -1 -3\n1 1 -3\n-1 1 -3\n4 0 1 2 3\n");
  }
  QString err;
  const auto obj = RoomGeometry::load(dir.filePath("wall.obj"), &err);
  INFO(err.toStdString());
  CHECK(obj.triangleCount() == 2);
  CHECK(obj.cast({0, 0, 0}, {0, 0, -1}) == Approx(2.f));
  const auto ply = RoomGeometry::load(dir.filePath("wall.ply"), &err);
  INFO(err.toStdString());
  CHECK(ply.triangleCount() == 2);
  CHECK(ply.cast({0, 0, 0}, {0, 0, -1}) == Approx(3.f));
  CHECK(RoomGeometry::load(dir.filePath("none.stl"), &err).empty());
}

TEST_CASE("Directions cast from a camera pose", "[threedim][room]")
{
  const auto room = RoomGeometry::box(2.f, 2.f, 2.f, 2.f, 1.f, 1.f);
  const std::vector<float> dirs{0, 0, -1, 1, 0, -1, 0, 1, 0, 0, -1, 0};
  auto r = castDirections(dirs, 3, 1, room, {0.5f, 0.f, 0.f}, {0.f, 0.f, 0.f});
  CHECK(r.hitFraction == Approx(1.));
  CHECK(r.points[0] == Approx(0.5f));
  CHECK(r.points[2] == Approx(-2.f));
  CHECK(r.points[5] == Approx(-1.f));
  CHECK(r.points[11] == 0.f);
  r = castDirections(dirs, 3, 1, room, {0.f, 0.f, 0.f}, {0.f, 90.f, 0.f});
  CHECK(r.points[0] == Approx(-2.f));
  CHECK(r.points[2] == Approx(0.f).margin(1e-5));
}

TEST_CASE("Room box: an open ceiling leaves the walls unbounded upward", "[threedim][room]")
{
  const auto room = RoomGeometry::box(3.f, 0.f, 3.f, 3.f, 1.3f, 0.f);
  const float s = std::sqrt(0.5f);
  CHECK(room.cast({0, 0, 0}, {0, s, -s}) == Approx(3.f / s));
  CHECK(room.cast({0, 0, 0}, {0, 1, 0}) < 0.f);
  CHECK(room.cast({0, 0, 0}, {0, 0, 1}) < 0.f);
}

TEST_CASE("Room geometry: a PLY whose faces come before its vertices loads", "[threedim][room]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  {
    QFile f{dir.filePath("wall.ply")};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write("ply\nformat ascii 1.0\nelement face 1\nproperty list uchar int vertex_indices\n"
            "element vertex 4\nproperty float x\nproperty float y\nproperty float z\n"
            "end_header\n4 0 1 2 3\n-1 -1 -3\n1 -1 -3\n1 1 -3\n-1 1 -3\n");
  }
  QString err;
  const auto ply = RoomGeometry::load(dir.filePath("wall.ply"), &err);
  INFO(err.toStdString());
  CHECK(ply.triangleCount() == 2);
  CHECK(ply.cast({0, 0, 0}, {0, 0, -1}) == Approx(3.f));
}

TEST_CASE("Room geometry: OBJ groups and relative indices load", "[threedim][room]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  {
    QFile f{dir.filePath("room.obj")};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write("o front\nv -1 -1 -2\nv 1 -1 -2\nv 1 1 -2\nf -3 -2 -1\n"
            "o floor\nv -1 -1 0\nv 1 -1 0\nv 0 -1 -1\nf 4 5 6\n");
  }
  QString err;
  const auto obj = RoomGeometry::load(dir.filePath("room.obj"), &err);
  INFO(err.toStdString());
  CHECK(obj.triangleCount() == 2);
  CHECK(obj.cast({0.5f, -0.5f, 0}, {0, 0, -1}) == Approx(2.f));
  CHECK(obj.cast({0, 0, -0.5f}, {0, -1, 0}) == Approx(1.f));
}

TEST_CASE("Room geometry: one Euler convention with the transform controls", "[threedim][room]")
{
  for(const std::array<float, 3> e : {std::array<float, 3>{10, 20, 30}, {-35, 80, 5}, {0, 0, 90}})
  {
    const QQuaternion q = QQuaternion::fromEulerAngles(e[0], e[1], e[2]);
    const auto R = eulerRotation(e);
    for(const QVector3D v : {QVector3D(1, 0, 0), QVector3D(0, 1, 0), QVector3D(0, 0, 1)})
    {
      const QVector3D a = q.rotatedVector(v);
      const Eigen::Vector3d b = R * Eigen::Vector3d(v.x(), v.y(), v.z());
      CHECK(a.x() == Approx(b.x()).margin(1e-5));
      CHECK(a.y() == Approx(b.y()).margin(1e-5));
      CHECK(a.z() == Approx(b.z()).margin(1e-5));
    }
  }
}
