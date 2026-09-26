#include <score/plugins/UuidKeySerialization.hpp>

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>

#include <catch2/catch_test_macros.hpp>

#include <cstring>

namespace
{
struct Tag;
using Key = UuidKey<Tag>;
const Key k_key{score::uuids::string_generator::compute("1b1d9f4c-3c5a-4d3e-9a0b-8f6e2c7d5a41")};
}

TEST_CASE("a UuidKey streams through QDataStream without an application", "[serialization]")
{
  QByteArray bytes;
  {
    QDataStream out{&bytes, QIODevice::WriteOnly};
    out << k_key;
  }
  // The format DataStreamReader writes: the 16 raw bytes of the uuid.
  REQUIRE(bytes.size() == 16);
  CHECK(std::memcmp(bytes.constData(), k_key.impl().data, 16) == 0);

  Key back;
  QDataStream in{bytes};
  in >> back;
  CHECK(back == k_key);
}
