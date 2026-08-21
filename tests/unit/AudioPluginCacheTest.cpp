// Tests for Media/AudioPluginCache.hpp — the versioned scan-cache blob and
// the cache-healing helpers.
//
// Regressions covered:
//  * The old caches were raw QVariant metatype blobs: a datastream layout
//    change made old blobs decode as garbage that canConvert() accepted
//    (the VST2 "vst_invalid_format" global was a workaround). The new blob
//    must fail *cleanly* on version bumps, truncation and corruption.
//  * Scan replies from other score instances used to be appended to
//    whichever instance owned the fixed notification port, duplicating
//    every plug-in on each run (observed: 200+ copies per CLAP plug-in).
//    deduplicate() heals such caches on load.
//  * A reply arriving after the 10s reaper had already declared a timeout
//    recorded a plug-in both as valid and as an "<Invalid>" marker;
//    dropShadowedInvalidEntries() resolves those pairs in favour of valid.

#include <Media/AudioPluginCache.hpp>

#include <QDataStream>
#include <QString>

#include <catch2/catch_test_macros.hpp>

namespace
{
struct TestInfo
{
  QString path;
  QString id;
  bool valid{};

  bool operator==(const TestInfo&) const = default;
};

QDataStream& operator<<(QDataStream& s, const TestInfo& i)
{
  return s << i.path << i.id << i.valid;
}
QDataStream& operator>>(QDataStream& s, TestInfo& i)
{
  return s >> i.path >> i.id >> i.valid;
}

TestInfo info(QString path, QString id, bool valid = true)
{
  return TestInfo{std::move(path), std::move(id), valid};
}

// -> QString is load-bearing, and is what the real callers get wrong: without
// it this deduces QStringBuilder<QStringBuilder<QString, char[2]>, QString>,
// whose outer half references the inner temporary of the return statement.
const auto key_of = [](const TestInfo& i) -> QString { return i.path + "|" + i.id; };
const auto path_of = [](const TestInfo& i) { return i.path; };
const auto is_valid = [](const TestInfo& i) { return i.valid; };
}

TEST_CASE("plugin cache roundtrip", "[pluginscan][cache]")
{
  const std::vector<TestInfo> src{
      info("/usr/lib/clap/a.clap", "org.a"), info("/usr/lib/clap/b.clap", "org.b"),
      info("/usr/lib/clap/broken.clap", "", false)};

  const auto blob = Media::serializePluginCache(3, src);
  const auto back = Media::deserializePluginCache<TestInfo>(3, blob);
  REQUIRE(back.has_value());
  REQUIRE(*back == src);
}

TEST_CASE("plugin cache: empty vector roundtrips", "[pluginscan][cache]")
{
  const auto blob = Media::serializePluginCache(1, std::vector<TestInfo>{});
  const auto back = Media::deserializePluginCache<TestInfo>(1, blob);
  REQUIRE(back.has_value());
  REQUIRE(back->empty());
}

TEST_CASE("plugin cache rejects version mismatch", "[pluginscan][cache]")
{
  const std::vector<TestInfo> src{info("/p", "x")};
  const auto blob = Media::serializePluginCache(3, src);
  REQUIRE(!Media::deserializePluginCache<TestInfo>(4, blob).has_value());
  REQUIRE(!Media::deserializePluginCache<TestInfo>(2, blob).has_value());
}

TEST_CASE("plugin cache rejects damage instead of returning garbage", "[pluginscan][cache]")
{
  const std::vector<TestInfo> src{info("/p1", "x"), info("/p2", "y")};
  auto blob = Media::serializePluginCache(3, src);

  SECTION("truncation")
  {
    blob.truncate(blob.size() / 2);
    REQUIRE(!Media::deserializePluginCache<TestInfo>(3, blob).has_value());
  }
  SECTION("trailing bytes")
  {
    blob.append("garbage");
    REQUIRE(!Media::deserializePluginCache<TestInfo>(3, blob).has_value());
  }
  SECTION("wrong magic")
  {
    blob[0] = 'X';
    REQUIRE(!Media::deserializePluginCache<TestInfo>(3, blob).has_value());
  }
  SECTION("empty")
  {
    REQUIRE(!Media::deserializePluginCache<TestInfo>(3, QByteArray{}).has_value());
  }
  SECTION("corrupt count")
  {
    // Header is magic(4) + version(4) + count(4): blast the count field
    blob[8] = (char)0xff;
    blob[9] = (char)0xff;
    blob[10] = (char)0xff;
    blob[11] = (char)0xff;
    REQUIRE(!Media::deserializePluginCache<TestInfo>(3, blob).has_value());
  }
}

TEST_CASE("deduplicate heals a 200x-duplicated cache", "[pluginscan][heal]")
{
  std::vector<TestInfo> cache;
  for(int run = 0; run < 200; run++)
  {
    cache.push_back(info("/usr/lib/clap/surge.clap", "org.surge"));
    cache.push_back(info("/usr/lib/clap/dexed.clap", "org.dexed"));
  }
  REQUIRE(cache.size() == 400);

  Media::deduplicate(cache, key_of);

  REQUIRE(cache.size() == 2);
  REQUIRE(cache[0].path == "/usr/lib/clap/surge.clap");
  REQUIRE(cache[1].path == "/usr/lib/clap/dexed.clap");
}

TEST_CASE("deduplicate keeps distinct plug-ins from one file", "[pluginscan][heal]")
{
  // One .clap file can host many plug-ins: same path, distinct ids
  std::vector<TestInfo> cache{
      info("/lsp.clap", "lsp.compressor"), info("/lsp.clap", "lsp.limiter"),
      info("/lsp.clap", "lsp.compressor")};

  Media::deduplicate(cache, key_of);

  REQUIRE(cache.size() == 2);
}

TEST_CASE("valid entries win over invalid markers for the same path", "[pluginscan][heal]")
{
  std::vector<TestInfo> cache{
      info("/a.clap", "", false),      // false timeout marker, recorded first
      info("/a.clap", "org.a", true),  // the real reply
      info("/dead.clap", "", false)};  // genuinely broken plug-in

  Media::dropShadowedInvalidEntries(cache, path_of, is_valid);

  REQUIRE(cache.size() == 2);
  REQUIRE(cache[0].id == "org.a");
  REQUIRE(cache[1].path == "/dead.clap"); // real invalid marker preserved
}

TEST_CASE("sanitizePluginCache combines all healing steps", "[pluginscan][heal]")
{
  std::vector<TestInfo> cache;
  // 50 runs' worth of cross-instance duplicates...
  for(int run = 0; run < 50; run++)
  {
    cache.push_back(info("/a.clap", "org.a"));
    cache.push_back(info("/b.clap", "org.b"));
  }
  // ...a timeout/reply double...
  cache.push_back(info("/a.clap", "", false));
  // ...an entry that decoded to an empty path...
  cache.push_back(info("", "junk"));
  // ...and a legitimately broken plug-in.
  cache.push_back(info("/dead.clap", "", false));

  Media::sanitizePluginCache(cache, key_of, path_of, is_valid);

  REQUIRE(cache.size() == 3);
  REQUIRE(cache[0] == info("/a.clap", "org.a"));
  REQUIRE(cache[1] == info("/b.clap", "org.b"));
  REQUIRE(cache[2] == info("/dead.clap", "", false));
}
