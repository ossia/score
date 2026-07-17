// Unit tests for the shared decoded-asset cache:
//  - Gfx::AssetTable          (Gfx/AssetTable.{hpp,cpp})
//  - score::gfx TextureLoader (Gfx/Graph/TextureLoader.{hpp,cpp})
//
// AssetTable is pure CPU-side logic (mutex + hash map + LRU list) and is
// fully testable without a QRhi. The TextureLoader decode helpers only need
// QImage. The upload helpers and TextureCache are exercised against QRhi's
// Null backend, which records the calls without touching a GPU.

#include <Gfx/AssetTable.hpp>
#include <Gfx/Graph/TextureLoader.hpp>

#include <ossia/detail/hash.hpp>

#include <QBuffer>
#include <QByteArray>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QTemporaryDir>

#include <private/qrhi_p.h>
#include <private/qrhinull_p.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

namespace
{
// QRhiNull's uploadTexture blits through QPainter-on-QImage, which needs a
// QGuiApplication (raster paint engine). Catch2 owns main(), so create it
// lazily, forcing the offscreen platform for headless CI.
void ensureApp()
{
  if(!QCoreApplication::instance())
  {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    static int argc = 1;
    static char arg0[] = "AssetTableTest";
    static char* argv[] = {arg0, nullptr};
    static QGuiApplication app(argc, argv);
  }
}

// A 10x10 RGBA8888 image: 10 * 10 * 4 = 400 bytes, scanlines already
// 4-byte-aligned so QImage::sizeInBytes() is exactly 400. The tests below
// rely on this to check the byte accounting precisely.
QImage makeImage(int w = 10, int h = 10, QColor color = Qt::red)
{
  QImage img(w, h, QImage::Format_RGBA8888);
  img.fill(color);
  return img;
}
constexpr std::size_t kImgBytes = 10 * 10 * 4;

std::shared_ptr<const std::vector<uint8_t>> makeBytes(std::size_t n, uint8_t fill = 0xAB)
{
  return std::make_shared<const std::vector<uint8_t>>(n, fill);
}
}

// ============================================================================
// AssetTable — staging
// ============================================================================

TEST_CASE("AssetTable: staged image is counted and starts cold", "[gfx][assettable]")
{
  Gfx::AssetTable table;
  CHECK(table.size() == 0);
  CHECK(table.totalBytes() == 0);
  CHECK(table.coldCount() == 0);

  table.stage(1, makeImage());

  CHECK(table.size() == 1);
  CHECK(table.totalBytes() == kImgBytes);
  // Regression for 2d8569018: a stage() that is never acquire()d must sit in
  // the cold LRU so it stays evictable instead of leaking for the session.
  CHECK(table.coldCount() == 1);
}

TEST_CASE("AssetTable: staged byte payload is counted and starts cold", "[gfx][assettable]")
{
  Gfx::AssetTable table;
  table.stage(1, makeBytes(100), "application/octet-stream");

  CHECK(table.size() == 1);
  CHECK(table.totalBytes() == 100);
  CHECK(table.coldCount() == 1);

  auto a = table.acquire(1);
  REQUIRE(a);
  CHECK(a->mime_type == "application/octet-stream");
  REQUIRE(a->bytes);
  CHECK(a->bytes->size() == 100);
  CHECK(a->image.isNull());
  CHECK(a->byte_size == 100);
}

TEST_CASE("AssetTable: image + bytes accounting adds up", "[gfx][assettable]")
{
  Gfx::AssetTable table;
  table.stage(1, makeImage());     // 400
  table.stage(2, makeBytes(100));  // 100

  CHECK(table.size() == 2);
  CHECK(table.totalBytes() == kImgBytes + 100);
  CHECK(table.coldCount() == 2);
}

TEST_CASE("AssetTable: stage is idempotent per content hash", "[gfx][assettable]")
{
  Gfx::AssetTable table;
  table.stage(1, makeImage(10, 10));
  // Hash contract: same hash = same bytes. A second stage under the same
  // hash must be a no-op even if the payload differs (caller bug).
  table.stage(1, makeImage(20, 20));

  CHECK(table.size() == 1);
  CHECK(table.totalBytes() == kImgBytes); // first payload wins
  auto a = table.acquire(1);
  REQUIRE(a);
  CHECK(a->image.width() == 10);

  // Idempotence also holds across payload kinds under one hash.
  table.stage(1, makeBytes(1000));
  CHECK(table.size() == 1);
  CHECK(table.totalBytes() == kImgBytes);
}

TEST_CASE("AssetTable: same content from different paths dedups by hash", "[gfx][assettable]")
{
  // Two glTF files referencing an identical baseColor.jpg produce the same
  // ossia::hash_bytes content hash regardless of path -> one decode staged.
  const std::vector<uint8_t> content_a{1, 2, 3, 4, 5, 6, 7, 8};
  const std::vector<uint8_t> content_b = content_a; // separate buffer, same bytes
  const std::vector<uint8_t> other{9, 9, 9, 9};

  const uint64_t h_a = ossia::hash_bytes(content_a.data(), content_a.size());
  const uint64_t h_b = ossia::hash_bytes(content_b.data(), content_b.size());
  const uint64_t h_other = ossia::hash_bytes(other.data(), other.size());
  CHECK(h_a == h_b);
  CHECK(h_a != h_other);

  Gfx::AssetTable table;
  table.stage(h_a, makeBytes(8)); // "decoded /sceneA/baseColor.jpg"
  table.stage(h_b, makeBytes(8)); // "decoded /sceneB/baseColor.jpg" -> no-op
  CHECK(table.size() == 1);

  table.stage(h_other, makeBytes(4));
  CHECK(table.size() == 2);
}

// ============================================================================
// AssetTable — acquire / peek / release
// ============================================================================

TEST_CASE("AssetTable: acquire miss returns null", "[gfx][assettable]")
{
  Gfx::AssetTable table;
  CHECK(table.acquire(42) == nullptr);
  CHECK(table.peek(42) == nullptr);
}

TEST_CASE("AssetTable: acquire bumps refcount and leaves the cold pool", "[gfx][assettable]")
{
  Gfx::AssetTable table;
  table.stage(1, makeImage());
  CHECK(table.coldCount() == 1);

  auto a = table.acquire(1);
  REQUIRE(a);
  CHECK(a->refcount == 1);
  CHECK(table.coldCount() == 0); // hot now
  CHECK(table.size() == 1);
  CHECK(table.totalBytes() == kImgBytes);

  auto b = table.acquire(1);
  REQUIRE(b);
  CHECK(b == a);           // same underlying DecodedAsset
  CHECK(a->refcount == 2); // second consumer
}

TEST_CASE("AssetTable: peek does not bump refcount nor warm the entry", "[gfx][assettable]")
{
  Gfx::AssetTable table;
  table.stage(1, makeImage());

  auto p = table.peek(1);
  REQUIRE(p);
  CHECK(p->refcount == 0);
  CHECK(table.coldCount() == 1); // still evictable
}

TEST_CASE("AssetTable: peeked pointer outlives eviction", "[gfx][assettable]")
{
  Gfx::AssetTable table;
  table.stage(1, makeImage(10, 10, Qt::blue));

  auto p = table.peek(1);
  REQUIRE(p);

  // Evict everything; the shared_ptr must keep the bytes alive on the
  // caller's side (ASAN would flag a use-after-free otherwise).
  table.trim(0);
  CHECK(table.size() == 0);
  CHECK(table.acquire(1) == nullptr);

  CHECK(p->image.width() == 10);
  CHECK(p->image.pixelColor(0, 0) == QColor(Qt::blue));
}

TEST_CASE("AssetTable: release at zero refcount moves the entry cold", "[gfx][assettable]")
{
  Gfx::AssetTable table;
  table.stage(1, makeImage());

  auto a = table.acquire(1);
  REQUIRE(a);
  CHECK(table.coldCount() == 0);

  table.release(1);
  CHECK(a->refcount == 0);
  CHECK(table.coldCount() == 1); // eligible for eviction again
  CHECK(table.size() == 1);      // but not evicted yet
}

TEST_CASE("AssetTable: release keeps the entry hot while other holders remain", "[gfx][assettable]")
{
  Gfx::AssetTable table;
  table.stage(1, makeImage());

  auto a = table.acquire(1);
  auto b = table.acquire(1);
  REQUIRE(a);
  CHECK(a->refcount == 2);

  table.release(1);
  CHECK(a->refcount == 1);
  CHECK(table.coldCount() == 0); // still hot

  table.release(1);
  CHECK(a->refcount == 0);
  CHECK(table.coldCount() == 1);
}

TEST_CASE("AssetTable: release is safe on missing hash and does not underflow", "[gfx][assettable]")
{
  Gfx::AssetTable table;
  table.release(42); // no-op, no crash

  table.stage(1, makeImage());
  auto a = table.acquire(1);
  REQUIRE(a);

  table.release(1);
  table.release(1); // extra release: refcount clamped at 0
  table.release(1);
  CHECK(a->refcount == 0);
  CHECK(table.coldCount() == 1); // not duplicated in the LRU either

  // The entry is still coherent: it can be re-acquired...
  auto b = table.acquire(1);
  REQUIRE(b);
  CHECK(b->refcount == 1);
  CHECK(table.coldCount() == 0);
  // ...and evicted cleanly after a single matching release.
  table.release(1);
  table.trim(0);
  CHECK(table.size() == 0);
  CHECK(table.totalBytes() == 0);
}

TEST_CASE("AssetTable: acquire resurrects a cold entry at zero cost", "[gfx][assettable]")
{
  Gfx::AssetTable table;
  table.stage(1, makeImage());

  auto a = table.acquire(1);
  table.release(1);
  CHECK(table.coldCount() == 1);

  auto b = table.acquire(1); // resurrect from the cold pool
  REQUIRE(b);
  CHECK(b == a);
  CHECK(b->refcount == 1);
  CHECK(table.coldCount() == 0);
  CHECK(table.totalBytes() == kImgBytes); // accounting stayed consistent
}

// ============================================================================
// AssetTable — trim / eviction
// ============================================================================

TEST_CASE("AssetTable: staged-but-never-acquired entry is evictable", "[gfx][assettable]")
{
  // Regression test for 2d8569018 ("make staged-but-never-acquired
  // AssetTable entries evictable"): before the fix these entries never
  // entered the cold LRU and leaked for the whole session.
  Gfx::AssetTable table;
  table.stage(1, makeImage());
  table.stage(2, makeBytes(100));

  const std::size_t evicted = table.trim(0);
  CHECK(evicted == kImgBytes + 100);
  CHECK(table.size() == 0);
  CHECK(table.totalBytes() == 0);
  CHECK(table.coldCount() == 0);
  CHECK(table.acquire(1) == nullptr);
  CHECK(table.acquire(2) == nullptr);
}

TEST_CASE("AssetTable: trim never evicts hot entries", "[gfx][assettable]")
{
  Gfx::AssetTable table;
  table.stage(1, makeImage());

  auto held = table.acquire(1);
  REQUIRE(held);

  const std::size_t evicted = table.trim(0); // zero budget, still nothing to evict
  CHECK(evicted == 0);
  CHECK(table.size() == 1);
  CHECK(table.totalBytes() == kImgBytes);
  CHECK(table.acquire(1) != nullptr);
}

TEST_CASE("AssetTable: trim evicts cold entries oldest-first until under budget", "[gfx][assettable]")
{
  Gfx::AssetTable table;
  table.stage(1, makeImage()); // oldest -> LRU tail
  table.stage(2, makeImage());
  table.stage(3, makeImage()); // newest -> LRU head

  // cold = 1200 bytes; budget 800 -> exactly one eviction (the oldest).
  const std::size_t evicted = table.trim(2 * kImgBytes);
  CHECK(evicted == kImgBytes);
  CHECK(table.size() == 2);
  CHECK(table.coldCount() == 2);
  CHECK(table.acquire(1) == nullptr); // oldest went first
  CHECK(table.acquire(2) != nullptr);
  CHECK(table.acquire(3) != nullptr);
}

TEST_CASE("AssetTable: release order defines LRU eviction order", "[gfx][assettable]")
{
  Gfx::AssetTable table;
  table.stage(1, makeImage());
  table.stage(2, makeImage());

  // Warm both, then cool them in a specific order: h1 first (older),
  // h2 last (newer). Eviction must take h1.
  auto a = table.acquire(1);
  auto b = table.acquire(2);
  table.release(1);
  table.release(2);
  CHECK(table.coldCount() == 2);

  table.trim(kImgBytes); // room for exactly one cold entry
  CHECK(table.size() == 1);
  CHECK(table.acquire(1) == nullptr); // released earlier -> evicted first
  CHECK(table.acquire(2) != nullptr);
}

TEST_CASE("AssetTable: trim with a large budget is a no-op", "[gfx][assettable]")
{
  Gfx::AssetTable table;
  table.stage(1, makeImage());
  CHECK(table.trim(1 << 20) == 0);
  CHECK(table.size() == 1);
}

TEST_CASE("AssetTable: re-stage after eviction works", "[gfx][assettable]")
{
  Gfx::AssetTable table;
  table.stage(1, makeImage(10, 10, Qt::red));
  table.trim(0);
  CHECK(table.acquire(1) == nullptr);

  // Caller re-decodes and restages under the same hash.
  table.stage(1, makeImage(10, 10, Qt::green));
  auto a = table.acquire(1);
  REQUIRE(a);
  CHECK(a->image.pixelColor(0, 0) == QColor(Qt::green));
  CHECK(table.totalBytes() == kImgBytes);
  CHECK(table.size() == 1);
}

TEST_CASE("AssetTable: zero-byte entries are not reclaimed by trim (current behavior)",
          "[gfx][assettable][!shouldfail]")
{
  // Documents a quirk: an entry whose payload is empty (null image, no
  // bytes) has byte_size == 0, so it never makes m_cold_bytes exceed any
  // budget and trim() cannot evict it. Harmless for memory (there are no
  // bytes) but the map/LRU slot stays behind. Marked !shouldfail so it
  // flips visibly if the behavior is ever changed to evict them.
  Gfx::AssetTable table;
  table.stage(1, QImage{});
  CHECK(table.size() == 1);
  CHECK(table.totalBytes() == 0);
  CHECK(table.coldCount() == 1);

  table.trim(0);
  CHECK(table.size() == 0); // fails today: the slot survives trim(0)
}

// ============================================================================
// AssetTable — maybeAutoTrim
// ============================================================================

TEST_CASE("AssetTable: maybeAutoTrim below the watermark is a no-op", "[gfx][assettable]")
{
  Gfx::AssetTable table;
  table.stage(1, makeImage());
  table.stage(2, makeImage());

  table.maybeAutoTrim(0.5f); // default watermark 0.80
  CHECK(table.size() == 2);

  table.maybeAutoTrim(0.79f);
  CHECK(table.size() == 2);
}

TEST_CASE("AssetTable: maybeAutoTrim with an empty cold pool is a no-op", "[gfx][assettable]")
{
  Gfx::AssetTable table;
  table.stage(1, makeImage());
  auto held = table.acquire(1); // everything hot

  table.maybeAutoTrim(0.99f);
  CHECK(table.size() == 1);
  CHECK(table.totalBytes() == kImgBytes);
}

TEST_CASE("AssetTable: maybeAutoTrim above the watermark trims toward the target", "[gfx][assettable]")
{
  Gfx::AssetTable table;
  table.stage(1, makeImage()); // oldest
  table.stage(2, makeImage());
  table.stage(3, makeImage()); // newest
  CHECK(table.coldCount() == 3);

  // utilization 1.0, target 0.5 -> budget = cold * 0.5 = 600 bytes.
  // Evicts h1 (1200 -> 800), then h2 (800 -> 400 <= 600), keeps h3.
  table.maybeAutoTrim(1.0f, 0.8f, 0.5f);
  CHECK(table.size() == 1);
  CHECK(table.coldCount() == 1);
  CHECK(table.acquire(1) == nullptr);
  CHECK(table.acquire(2) == nullptr);
  CHECK(table.acquire(3) != nullptr);
}

TEST_CASE("AssetTable: maybeAutoTrim never touches hot entries", "[gfx][assettable]")
{
  Gfx::AssetTable table;
  table.stage(1, makeImage());
  table.stage(2, makeImage());
  auto held = table.acquire(1);

  table.maybeAutoTrim(1.0f, 0.8f, 0.0f); // target 0 -> drain the cold pool
  CHECK(table.acquire(2) == nullptr);    // cold entry gone
  CHECK(table.size() == 1);              // hot entry untouched
  CHECK(table.totalBytes() == kImgBytes);
}

// ============================================================================
// TextureLoader — CPU decode helpers (no QRhi needed)
// ============================================================================

namespace
{
// Save a QImage as a real PNG file and return the payload too.
struct PngFixture
{
  QTemporaryDir dir;
  QString path;
  QByteArray bytes;

  explicit PngFixture(const QImage& img)
  {
    REQUIRE(dir.isValid());
    path = dir.filePath("asset.png");
    REQUIRE(img.save(path, "PNG"));

    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    REQUIRE(img.save(&buf, "PNG"));
  }
};
}

TEST_CASE("TextureLoader: decodeImageFromPath decodes and canonicalizes to RGBA8888",
          "[gfx][textureloader]")
{
  // Source deliberately NOT RGBA8888 so the conversion branch runs.
  QImage src(8, 4, QImage::Format_ARGB32);
  src.fill(Qt::transparent);
  src.setPixelColor(0, 0, QColor(255, 0, 0, 255));
  src.setPixelColor(7, 3, QColor(0, 0, 255, 255));
  PngFixture png(src);

  auto decoded = score::gfx::decodeImageFromPath(png.path);
  REQUIRE(decoded.has_value());
  CHECK(decoded->image.format() == QImage::Format_RGBA8888);
  CHECK(decoded->image.width() == 8);
  CHECK(decoded->image.height() == 4);
  CHECK(decoded->image.pixelColor(0, 0) == QColor(255, 0, 0, 255));
  CHECK(decoded->image.pixelColor(7, 3) == QColor(0, 0, 255, 255));
  CHECK(decoded->debug_name == png.path);
}

TEST_CASE("TextureLoader: decodeImageFromPath fails cleanly on a missing file",
          "[gfx][textureloader]")
{
  CHECK_FALSE(score::gfx::decodeImageFromPath(
                  QStringLiteral("/nonexistent/definitely-not-here.png"))
                  .has_value());
  CHECK_FALSE(score::gfx::decodeImageFromPath(QString{}).has_value());
}

TEST_CASE("TextureLoader: decodeImageFromMemory honors the MIME hint", "[gfx][textureloader]")
{
  PngFixture png(makeImage(6, 6, Qt::green));

  SECTION("full MIME type: the image/ prefix is stripped")
  {
    auto decoded
        = score::gfx::decodeImageFromMemory(png.bytes, QStringLiteral("image/png"));
    REQUIRE(decoded.has_value());
    CHECK(decoded->image.format() == QImage::Format_RGBA8888);
    CHECK(decoded->image.width() == 6);
    CHECK(decoded->image.pixelColor(0, 0) == QColor(Qt::green));
    CHECK(decoded->debug_name == QStringLiteral("blob:image/png"));
  }

  SECTION("bare format hint")
  {
    auto decoded = score::gfx::decodeImageFromMemory(png.bytes, QStringLiteral("png"));
    REQUIRE(decoded.has_value());
    CHECK(decoded->image.width() == 6);
  }

  SECTION("no hint: format autodetection")
  {
    auto decoded = score::gfx::decodeImageFromMemory(png.bytes, QString{});
    REQUIRE(decoded.has_value());
    CHECK(decoded->image.width() == 6);
  }
}

TEST_CASE("TextureLoader: decodeImageFromMemory fails cleanly on garbage", "[gfx][textureloader]")
{
  const QByteArray garbage("this is definitely not an image payload");
  CHECK_FALSE(
      score::gfx::decodeImageFromMemory(garbage, QStringLiteral("image/png")).has_value());
  CHECK_FALSE(score::gfx::decodeImageFromMemory(garbage, QString{}).has_value());
  CHECK_FALSE(score::gfx::decodeImageFromMemory(QByteArray{}, QString{}).has_value());
}

// ============================================================================
// TextureLoader — GPU upload + TextureCache, on QRhi's Null backend.
//
// The Null backend implements the full QRhi contract without a device, so
// texture creation, upload recording and mip generation run for real; only
// the actual GPU work is skipped. Behavior specific to a live driver
// (real memory allocation failures, sampler behavior, actual mip contents)
// still needs a GPU integration test.
// ============================================================================

namespace
{
std::unique_ptr<QRhi> makeNullRhi()
{
  ensureApp();
  QRhiNullInitParams params;
  return std::unique_ptr<QRhi>(QRhi::create(QRhi::Null, &params));
}

// Submit a batch so its recorded commands are consumed (Null backend
// executes them as no-ops); keeps the update-batch pool clean.
void submit(QRhi& rhi, QRhiResourceUpdateBatch* batch)
{
  QRhiCommandBuffer* cb{};
  REQUIRE(rhi.beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess);
  cb->resourceUpdate(batch);
  rhi.endOffscreenFrame();
}
}

TEST_CASE("TextureLoader: uploadImageToTexture creates a mip-mapped RGBA8 texture",
          "[gfx][textureloader][rhi]")
{
  auto rhi = makeNullRhi();
  REQUIRE(rhi);
  auto* batch = rhi->nextResourceUpdateBatch();
  REQUIRE(batch);

  const QImage img = makeImage(16, 8);

  SECTION("linear")
  {
    std::unique_ptr<QRhiTexture> tex(score::gfx::uploadImageToTexture(
        *rhi, *batch, img, false, QStringLiteral("test-tex")));
    REQUIRE(tex);
    CHECK(tex->format() == QRhiTexture::RGBA8);
    CHECK(tex->pixelSize() == QSize(16, 8));
    CHECK(tex->flags().testFlag(QRhiTexture::MipMapped));
    CHECK(tex->flags().testFlag(QRhiTexture::UsedWithGenerateMips));
    CHECK_FALSE(tex->flags().testFlag(QRhiTexture::sRGB));
    CHECK(tex->name() == QByteArray("test-tex"));
    submit(*rhi, batch);
  }

  SECTION("sRGB flag set when requested")
  {
    std::unique_ptr<QRhiTexture> tex(
        score::gfx::uploadImageToTexture(*rhi, *batch, img, true));
    REQUIRE(tex);
    CHECK(tex->flags().testFlag(QRhiTexture::sRGB));
    submit(*rhi, batch);
  }

  SECTION("null image returns nullptr")
  {
    CHECK(score::gfx::uploadImageToTexture(*rhi, *batch, QImage{}, false) == nullptr);
    batch->release();
  }
}

TEST_CASE("TextureLoader: one-shot loadAndUploadTexture helpers", "[gfx][textureloader][rhi]")
{
  auto rhi = makeNullRhi();
  REQUIRE(rhi);
  auto* batch = rhi->nextResourceUpdateBatch();
  REQUIRE(batch);

  PngFixture png(makeImage(12, 12, Qt::yellow));

  SECTION("from path")
  {
    std::unique_ptr<QRhiTexture> tex(
        score::gfx::loadAndUploadTexture(*rhi, *batch, png.path, false));
    REQUIRE(tex);
    CHECK(tex->pixelSize() == QSize(12, 12));
    submit(*rhi, batch);
  }

  SECTION("from memory")
  {
    std::unique_ptr<QRhiTexture> tex(score::gfx::loadAndUploadTexture(
        *rhi, *batch, png.bytes, QStringLiteral("image/png"), false));
    REQUIRE(tex);
    CHECK(tex->pixelSize() == QSize(12, 12));
    submit(*rhi, batch);
  }

  SECTION("decode failure returns nullptr")
  {
    CHECK(score::gfx::loadAndUploadTexture(
              *rhi, *batch, QStringLiteral("/nope/missing.png"), false)
          == nullptr);
    CHECK(score::gfx::loadAndUploadTexture(
              *rhi, *batch, QByteArray("garbage"), QStringLiteral("image/png"), false)
          == nullptr);
    batch->release();
  }
}

TEST_CASE("TextureCache: path acquisitions are deduplicated", "[gfx][textureloader][rhi]")
{
  auto rhi = makeNullRhi();
  REQUIRE(rhi);
  auto* batch = rhi->nextResourceUpdateBatch();
  REQUIRE(batch);

  PngFixture png(makeImage(4, 4));

  {
    score::gfx::TextureCache cache;
    CHECK(cache.size() == 0);

    auto* t1 = cache.acquireFromPath(*rhi, *batch, png.path, false);
    REQUIRE(t1);
    CHECK(cache.size() == 1);

    // Second acquisition: cache hit, no re-decode/re-upload, same texture.
    auto* t2 = cache.acquireFromPath(*rhi, *batch, png.path, false);
    CHECK(t2 == t1);
    CHECK(cache.size() == 1);

    // Same path but different sRGB flag: distinct GPU object, both cached.
    auto* t3 = cache.acquireFromPath(*rhi, *batch, png.path, true);
    REQUIRE(t3);
    CHECK(t3 != t1);
    CHECK(t3->flags().testFlag(QRhiTexture::sRGB));
    CHECK(cache.size() == 2);

    // Empty path is rejected outright.
    CHECK(cache.acquireFromPath(*rhi, *batch, QString{}, false) == nullptr);
    CHECK(cache.size() == 2);

    // Submit the recorded uploads while the textures are alive — outside an
    // active frame QRhiResource::deleteLater() deletes immediately, so
    // clear() before submission would leave dangling texture pointers in
    // the batch (matches real usage: uploads are consumed in-frame).
    submit(*rhi, batch);

    cache.clear();
    CHECK(cache.size() == 0);
  } // dtor runs clear() again: must be safe
}

TEST_CASE("TextureCache: decode failures are not cached and can be retried",
          "[gfx][textureloader][rhi]")
{
  auto rhi = makeNullRhi();
  REQUIRE(rhi);
  auto* batch = rhi->nextResourceUpdateBatch();
  REQUIRE(batch);

  score::gfx::TextureCache cache;
  const QString missing = QStringLiteral("/nope/still-missing.png");
  CHECK(cache.acquireFromPath(*rhi, *batch, missing, false) == nullptr);
  CHECK(cache.size() == 0); // failure not cached...

  // ...so once the file exists, the same key succeeds.
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString path = dir.filePath("late.png");
  REQUIRE(makeImage(3, 3).save(path, "PNG"));
  CHECK(cache.acquireFromPath(*rhi, *batch, missing, false) == nullptr); // still missing
  auto* t = cache.acquireFromPath(*rhi, *batch, path, false);
  CHECK(t != nullptr);
  CHECK(cache.size() == 1);

  submit(*rhi, batch);
}

TEST_CASE("TextureCache: memory acquisitions key on the caller's content hash",
          "[gfx][textureloader][rhi]")
{
  auto rhi = makeNullRhi();
  REQUIRE(rhi);
  auto* batch = rhi->nextResourceUpdateBatch();
  REQUIRE(batch);

  PngFixture red(makeImage(5, 5, Qt::red));
  PngFixture blue(makeImage(5, 5, Qt::blue));
  const uint64_t h_red = ossia::hash_bytes(red.bytes.constData(), red.bytes.size());
  const uint64_t h_blue = ossia::hash_bytes(blue.bytes.constData(), blue.bytes.size());
  REQUIRE(h_red != h_blue);

  score::gfx::TextureCache cache;
  auto* t1 = cache.acquireFromMemory(
      *rhi, *batch, red.bytes, QStringLiteral("image/png"), h_red, false);
  REQUIRE(t1);
  CHECK(cache.size() == 1);

  // Same hash -> cache hit; the bytes are not even looked at again
  // (hash identity is the contract).
  auto* t2 = cache.acquireFromMemory(
      *rhi, *batch, blue.bytes, QStringLiteral("image/png"), h_red, false);
  CHECK(t2 == t1);
  CHECK(cache.size() == 1);

  // Different hash -> new decode + upload.
  auto* t3 = cache.acquireFromMemory(
      *rhi, *batch, blue.bytes, QStringLiteral("image/png"), h_blue, false);
  REQUIRE(t3);
  CHECK(t3 != t1);
  CHECK(cache.size() == 2);

  // Same hash, different sRGB flag -> separate entry.
  auto* t4 = cache.acquireFromMemory(
      *rhi, *batch, red.bytes, QStringLiteral("image/png"), h_red, true);
  REQUIRE(t4);
  CHECK(t4 != t1);
  CHECK(cache.size() == 3);

  // Garbage bytes: failure, not cached.
  auto* t5 = cache.acquireFromMemory(
      *rhi, *batch, QByteArray("garbage"), QStringLiteral("image/png"), 0xDEAD, false);
  CHECK(t5 == nullptr);
  CHECK(cache.size() == 3);

  submit(*rhi, batch);
}
